#include "app/BatchProcessor.h"
#include "app/Command.h"
#include "runtime/FileMetadataCache.h"
#include "support/Diagnostic.h"
#include "support/FuzzTempDir.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <fuzzer/FuzzedDataProvider.h>
#include <optional>
#include <string>

namespace {

constexpr std::size_t kMaxTemplateBytes = 256 * 1024;
constexpr std::size_t kMaxBatchJsonBytes = 64 * 1024;
constexpr const char* kSettingsExtensions[] = {".yaml", ".json", ".toml", ".ini"};

enum class OutputRoute {
    Stdout,
    SingleFile,
    Directory,
};

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::binary);
    file << content;
}

void seed_support_files(const std::filesystem::path& root) {
    write_file(root / "header.txt", "Header {{ name }}\n");
    write_file(root / "partial.pbt", "<{{ item }}>");
    write_file(root / "nested" / "child.pbt", "Child {{ name }}\n");
}

OutputRoute pick_output_route(FuzzedDataProvider& provider) {
    switch (provider.ConsumeIntegralInRange(0, 2)) {
    case 0:
        return OutputRoute::Stdout;
    case 1:
        return OutputRoute::SingleFile;
    case 2:
    default:
        return OutputRoute::Directory;
    }
}

std::string random_rule_arg(FuzzedDataProvider& provider, const std::filesystem::path& root) {
    switch (provider.ConsumeIntegralInRange(0, 8)) {
    case 0:
        return std::string("strict_variables=") + (provider.ConsumeBool() ? "true" : "false");
    case 1:
        return std::string("allow_includes=") + (provider.ConsumeBool() ? "true" : "false");
    case 2:
        return "max_include_depth=" + std::to_string(provider.ConsumeIntegralInRange(0, 8));
    case 3:
        return "max_output_size_bytes=" + std::to_string(provider.ConsumeIntegralInRange(64, 8192));
    case 4:
        return "max_loop_iteration=" + std::to_string(provider.ConsumeIntegralInRange(1, 100));
    case 5:
        return "lua_instruction_limit=" + std::to_string(provider.ConsumeIntegralInRange(1000, 50000));
    case 6:
        return "lua_memory_limit_bytes=" + std::to_string(provider.ConsumeIntegralInRange(512 * 1024, 2 * 1024 * 1024));
    case 7:
        return std::string("trim=") + (provider.ConsumeBool() ? "true" : "false");
    case 8:
    default:
        return "include_path=" + root.string();
    }
}

std::string random_define_arg(FuzzedDataProvider& provider) {
    const std::string name = provider.ConsumeRandomLengthString(12);
    if (name.empty()) {
        return "prefix=Batch";
    }
    return name + "=" + provider.ConsumeRandomLengthString(24);
}

void populate_cli_overrides(FuzzedDataProvider& provider, prebyte::Command& command,
                            const std::filesystem::path& root) {
    const std::size_t rule_count = provider.ConsumeIntegralInRange(0, 3);
    for (std::size_t index = 0; index < rule_count; ++index) {
        command.rule_args.push_back(random_rule_arg(provider, root));
    }

    const std::size_t define_count = provider.ConsumeIntegralInRange(0, 2);
    for (std::size_t index = 0; index < define_count; ++index) {
        command.define_args.push_back(random_define_arg(provider));
    }

    if (provider.ConsumeBool()) {
        command.profile_names.push_back(provider.ConsumeRandomLengthString(12));
    }
    if (provider.ConsumeBool()) {
        command.ignore_names.push_back(provider.ConsumeRandomLengthString(12));
    }
    if (provider.ConsumeBool()) {
        command.include_paths.push_back(root);
    }
    if (provider.ConsumeBool()) {
        command.render_args.push_back(provider.ConsumeRandomLengthString(16));
    }

    command.benchmark = provider.ConsumeBool();
    command.debug = provider.ConsumeBool();
}

std::optional<std::filesystem::path> maybe_write_settings(FuzzedDataProvider& provider,
                                                          const std::filesystem::path& root) {
    if (!provider.ConsumeBool()) {
        return std::nullopt;
    }

    const int extension_index = provider.ConsumeIntegralInRange(0, 3);
    const std::string content = provider.ConsumeRandomLengthString(512);
    const std::filesystem::path path =
        root / (std::string("settings") + kSettingsExtensions[extension_index]);
    write_file(path, content.empty() ? "name = Ada\n" : content);
    return path;
}

std::string fallback_batch_json() {
    return R"([{"name":"Ada","value":"one"},{"name":"Grace","value":"two"}])";
}

std::string normalize_batch_json(std::string batch_json) {
    if (batch_json.empty()) {
        return fallback_batch_json();
    }
    if (batch_json.size() > kMaxBatchJsonBytes) {
        batch_json.resize(kMaxBatchJsonBytes);
    }
    return batch_json;
}

std::string normalize_template_source(std::string template_source) {
    if (template_source.empty()) {
        return "{{ name }}|{{ value }}|{{ greeting }}";
    }
    if (template_source.size() > kMaxTemplateBytes) {
        template_source.resize(kMaxTemplateBytes);
    }
    return template_source;
}

void apply_output_route(OutputRoute route, prebyte::Command& command, const std::filesystem::path& root) {
    switch (route) {
    case OutputRoute::Stdout:
        command.output_path = std::nullopt;
        break;
    case OutputRoute::SingleFile:
        command.output_path = root / "out.txt";
        break;
    case OutputRoute::Directory:
        command.output_path = root / "out/";
        break;
    }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        return 0;
    }

    FuzzedDataProvider provider(data, size);
    prebyte::FileMetadataCache::instance().clear();

    const OutputRoute output_route = pick_output_route(provider);
    const bool use_pbt_extension = provider.ConsumeBool();
    const std::string partial_name = provider.ConsumeRandomLengthString(48);
    const std::string partial_source = provider.ConsumeRandomLengthString(512);
    const std::string batch_json = normalize_batch_json(provider.ConsumeRandomLengthString(4096));

    FuzzTempDir temp_dir;
    const std::filesystem::path root = temp_dir.path();
    seed_support_files(root);

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    populate_cli_overrides(provider, command, root);
    command.settings_path = maybe_write_settings(provider, root);
    apply_output_route(output_route, command, root);

    const std::string template_source = normalize_template_source(provider.ConsumeRemainingBytesAsString());
    const std::filesystem::path template_path =
        root / (use_pbt_extension ? "template.pbt" : "template.txt");
    write_file(template_path, template_source);
    write_file(root / "data.json", batch_json);
    command.input_path = template_path;
    command.batch_path = root / "data.json";

    if (!partial_name.empty()) {
        write_file(root / partial_name, partial_source.empty() ? "Partial {{ name }}\n" : partial_source);
    }

    try {
        prebyte::BatchProcessor processor;
        (void)processor.execute(command);
    } catch (const prebyte::DiagnosticError&) {
    } catch (const std::exception&) {
    }

    return 0;
}
