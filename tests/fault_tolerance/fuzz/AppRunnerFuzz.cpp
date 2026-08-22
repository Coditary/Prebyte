#include "app/AppRunner.h"
#include "app/Command.h"
#include "support/FuzzRuntimeReset.h"
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
constexpr const char* kSettingsExtensions[] = {".yaml", ".json", ".toml", ".ini"};
constexpr const char* kExplainTopics[] = {"rule", "rules", "ignore", "profile", "truthiness", "lua", "args", "unknown"};

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

std::string random_rule_arg(FuzzedDataProvider& provider, const std::filesystem::path& root) {
    switch (provider.ConsumeIntegralInRange(0, 10)) {
    case 0:
        return std::string("strict_variables=") + (provider.ConsumeBool() ? "true" : "false");
    case 1:
        return std::string("allow_includes=") + (provider.ConsumeBool() ? "true" : "false");
    case 2:
        return "max_include_depth=" + std::to_string(provider.ConsumeIntegralInRange(0, 8));
    case 3:
        return "max_render_time_ms=" + std::to_string(provider.ConsumeIntegralInRange(0, 50));
    case 4:
        return "max_output_size_bytes=" + std::to_string(provider.ConsumeIntegralInRange(64, 8192));
    case 5:
        return "max_loop_iteration=" + std::to_string(provider.ConsumeIntegralInRange(1, 100));
    case 6:
        return "lua_instruction_limit=" + std::to_string(provider.ConsumeIntegralInRange(1000, 50000));
    case 7:
        return "lua_memory_limit_bytes=" + std::to_string(provider.ConsumeIntegralInRange(512 * 1024, 2 * 1024 * 1024));
    case 8:
        return std::string("output_encoding=") + (provider.ConsumeBool() ? "utf-16" : "utf-8");
    case 9:
        return std::string("error_on_false_input=") + (provider.ConsumeBool() ? "true" : "false");
    case 10:
    default:
        return "include_path=" + root.string();
    }
}

std::string random_define_arg(FuzzedDataProvider& provider) {
    const std::string name = provider.ConsumeRandomLengthString(12);
    if (name.empty()) {
        return "name=Ada";
    }
    return name + "=" + provider.ConsumeRandomLengthString(24);
}

prebyte::CommandMode pick_mode(FuzzedDataProvider& provider) {
    switch (provider.ConsumeIntegralInRange(0, 15)) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
        return prebyte::CommandMode::Render;
    case 10:
        return prebyte::CommandMode::ListRules;
    case 11:
        return prebyte::CommandMode::ListVars;
    case 12:
        return prebyte::CommandMode::ListProfiles;
    case 13:
        return prebyte::CommandMode::ListIgnores;
    case 14:
        return prebyte::CommandMode::Explain;
    case 15:
        return prebyte::CommandMode::Help;
    default:
        return prebyte::CommandMode::Version;
    }
}

void populate_cli_overrides(FuzzedDataProvider& provider, prebyte::Command& command,
                            const std::filesystem::path& root) {
    const std::size_t rule_count = provider.ConsumeIntegralInRange(0, 4);
    for (std::size_t index = 0; index < rule_count; ++index) {
        command.rule_args.push_back(random_rule_arg(provider, root));
    }

    const std::size_t define_count = provider.ConsumeIntegralInRange(0, 3);
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
        command.include_paths.push_back(root / "nested");
    }

    const std::size_t render_arg_count = provider.ConsumeIntegralInRange(0, 2);
    for (std::size_t index = 0; index < render_arg_count; ++index) {
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

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        return 0;
    }

    FuzzedDataProvider provider(data, size);
    fuzz_reset_runtime_state();

    const prebyte::CommandMode mode = pick_mode(provider);
    const bool use_inline_input = provider.ConsumeBool();
    const std::string partial_name = provider.ConsumeRandomLengthString(48);
    const std::string partial_source = provider.ConsumeRandomLengthString(512);

    prebyte::Command command;
    command.mode = mode;

    FuzzTempDir temp_dir;
    const std::filesystem::path root = temp_dir.path();
    populate_cli_overrides(provider, command, root);
    command.settings_path = maybe_write_settings(provider, root);

    if (mode == prebyte::CommandMode::Explain) {
        command.explain_topic = kExplainTopics[provider.ConsumeIntegralInRange(0, 7)];
    }

    const bool write_output_file = provider.ConsumeBool();
    const std::string template_source = provider.ConsumeRemainingBytesAsString();

    if (template_source.size() > kMaxTemplateBytes) {
        return 0;
    }

    seed_support_files(root);

    if (!partial_name.empty()) {
        write_file(root / partial_name, partial_source.empty() ? "Partial {{ name }}\n" : partial_source);
    }

    if (mode == prebyte::CommandMode::Render) {
        if (template_source.empty() && !use_inline_input) {
            write_file(root / "main.pbt", "Hello {{ name }}\n");
        } else if (!template_source.empty()) {
            write_file(root / "main.pbt", template_source);
        }

        const std::filesystem::path template_path = root / "main.pbt";
        command.input_path = template_path;

        if (use_inline_input && !template_source.empty()) {
            command.inline_input = template_source;
        }

        if (write_output_file) {
            command.output_path = root / "output.txt";
        }
    }

    try {
        prebyte::AppRunner runner;
        (void)runner.execute(command);
    } catch (const prebyte::DiagnosticError&) {
    } catch (const std::exception&) {
    }

    return 0;
}
