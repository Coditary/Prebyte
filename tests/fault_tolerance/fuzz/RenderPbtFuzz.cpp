#include "config/ConfigTypes.h"
#include "config/RuleResolver.h"
#include "datatypes/Data.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/ExpressionEvaluator.h"
#include "support/FuzzRuntimeReset.h"
#include "runtime/IncludeResolver.h"
#include "runtime/Renderer.h"
#include "support/Diagnostic.h"
#include "support/FuzzTempDir.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <fuzzer/FuzzedDataProvider.h>
#include <limits>
#include <string>

namespace {

constexpr std::size_t kMaxTemplateBytes = 256 * 1024;

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::binary);
    file << content;
}

void seed_support_files(const std::filesystem::path& root) {
    write_file(root / "header.txt", "Header {{ name }}\n");
    write_file(root / "partial.pbt", "<{{ loop.index }}:{{ item }}>\n");
    write_file(root / "nested" / "child.pbt", "Child {{ name }}\n");
    write_file(root / "cycle_a.pbt", "{{ include \"cycle_b.pbt\" }}");
    write_file(root / "cycle_b.pbt", "{{ include \"cycle_a.pbt\" }}");
}

prebyte::EffectiveSettings make_settings(FuzzedDataProvider& provider) {
    prebyte::EffectiveSettings settings;
    settings.strict_variables = provider.ConsumeBool();
    settings.allow_includes = provider.ConsumeBool();
    settings.replace_tabs = provider.ConsumeBool();
    settings.trim = provider.ConsumeBool();
    settings.max_include_depth = provider.ConsumeIntegralInRange<std::size_t>(0, 8);
    settings.lua_instruction_limit = provider.ConsumeIntegralInRange<std::size_t>(1000, 100000);
    settings.lua_memory_limit_bytes = provider.ConsumeIntegralInRange<std::size_t>(512 * 1024, 4 * 1024 * 1024);
    settings.max_output_size_bytes = provider.ConsumeIntegralInRange<std::size_t>(1024, 4 * 1024 * 1024);
    settings.max_loop_iteration = provider.ConsumeIntegralInRange<std::size_t>(1, 1000);
    settings.max_render_time_ms =
        provider.ConsumeBool() ? 0 : std::numeric_limits<std::size_t>::max();
    return settings;
}

void seed_session(FuzzedDataProvider& provider, prebyte::RenderSession& session,
                  const prebyte::EffectiveSettings& settings) {
    session.variables.set("name", provider.ConsumeRandomLengthString(32));
    session.variables.set("enabled", provider.ConsumeBool() ? "true" : "false");
    session.variables.set("fromSettings", provider.ConsumeRandomLengthString(16));
    session.args = {provider.ConsumeRandomLengthString(16), provider.ConsumeRandomLengthString(16)};

    prebyte::Data::Array items;
    items.push_back(prebyte::Data(provider.ConsumeRandomLengthString(8)));
    items.push_back(prebyte::Data(provider.ConsumeRandomLengthString(8)));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    prebyte::Data::Array groups;
    prebyte::Data::Map group_map;
    group_map["featured"] = prebyte::Data(true);
    groups.push_back(prebyte::Data(std::move(group_map)));
    session.variables.set_value("groups", prebyte::Value::list(std::move(groups)));

    if (settings.max_render_time_ms == 0) {
        session.start_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        return 0;
    }

    FuzzedDataProvider provider(data, size);
    fuzz_reset_runtime_state();

    const prebyte::EffectiveSettings settings = make_settings(provider);
    const bool seed_session_data = provider.ConsumeBool();
    const std::string partial_name = provider.ConsumeRandomLengthString(48);
    const std::string partial_source = provider.ConsumeRandomLengthString(512);

    prebyte::RenderSession session;
    if (seed_session_data) {
        seed_session(provider, session, settings);
    }

    const std::string template_source = provider.ConsumeRemainingBytesAsString();

    if (template_source.empty() || template_source.size() > kMaxTemplateBytes) {
        return 0;
    }

    FuzzTempDir temp_dir;
    const std::filesystem::path root = temp_dir.path();
    seed_support_files(root);

    const std::filesystem::path template_path = root / "main.pbt";
    write_file(template_path, template_source);

    if (!partial_name.empty()) {
        write_file(root / partial_name, partial_source.empty() ? "Partial {{ name }}\n" : partial_source);
    }

    prebyte::EffectiveSettings active_settings = settings;
    active_settings.include_paths = {root, root / "nested"};

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    try {
        (void)renderer.render_source(template_source, active_settings, template_path, session);
    } catch (const prebyte::DiagnosticError&) {
    } catch (const std::exception&) {
    }

    return 0;
}
