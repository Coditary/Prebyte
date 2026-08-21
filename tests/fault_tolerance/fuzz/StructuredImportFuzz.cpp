#include "app/AppRunner.h"
#include "app/Command.h"
#include "runtime/FileMetadataCache.h"
#include "support/Diagnostic.h"
#include "support/FuzzTempDir.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <fuzzer/FuzzedDataProvider.h>
#include <string>

namespace {

constexpr std::size_t kMaxDataBytes = 64 * 1024;
constexpr std::size_t kMaxTemplateBytes = 8 * 1024;

struct ImportFormatSpec {
    const char* extension;
    const char* variable_name;
    const char* fallback_data;
    const char* fallback_template;
};

constexpr ImportFormatSpec kImportFormats[] = {
    {".json", "data", R"({"name":"Ada","items":["A","B"]})", "{{ data.name }}|{{ data.items[1] }}"},
    {".yaml", "data", "name: Ada\nitems:\n  - A\n  - B\n", "{{ data.name }}|{{ data.items[1] }}"},
    {".toml", "data", "[server]\nhost=\"localhost\"\nport=8080\n", "{{ data.server.host }}:{{ data.server.port }}"},
    {".ini", "data", "[server]\nhost = localhost\nport = 8080\n", "{{ data.server.host }}:{{ data.server.port }}"},
    {".env", "data", "NAME=Ada\nROLE=admin\n", "{{ data.NAME }}:{{ data.ROLE }}"},
};

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::string fallback_or_custom(const std::string& custom, const char* fallback) {
    return custom.empty() ? std::string(fallback) : custom;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        return 0;
    }

    FuzzedDataProvider provider(data, size);
    prebyte::FileMetadataCache::instance().clear();

    const int format_index = provider.ConsumeIntegralInRange(0, 4);
    const ImportFormatSpec& format = kImportFormats[format_index];
    const bool use_custom_template = provider.ConsumeBool();
    const bool use_lua_access = provider.ConsumeBool();
    const bool apply_strict_rule = provider.ConsumeBool();
    const bool strict_variables = provider.ConsumeBool();
    const std::string custom_template = use_custom_template ? provider.ConsumeRandomLengthString(kMaxTemplateBytes) : std::string();
    const std::string data_content = provider.ConsumeRemainingBytesAsString();

    if (data_content.size() > kMaxDataBytes) {
        return 0;
    }

    FuzzTempDir temp_dir;
    const std::filesystem::path root = temp_dir.path();
    const std::filesystem::path data_path = root / ("import" + std::string(format.extension));
    write_file(data_path, fallback_or_custom(data_content, format.fallback_data));

    std::string template_source = fallback_or_custom(custom_template, format.fallback_template);
    if (use_lua_access && template_source == format.fallback_template) {
        template_source = std::string("{{ ") + format.variable_name + ".name }} {{ lua \"return "
            + format.variable_name + ".name or " + format.variable_name + ".NAME or ''\" }}";
    }

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = template_source;
    command.define_args = {std::string(format.variable_name) + "=@" + data_path.string()};
    if (apply_strict_rule) {
        command.rule_args.push_back(std::string("strict_variables=") + (strict_variables ? "true" : "false"));
    }

    try {
        prebyte::AppRunner runner;
        (void)runner.execute(command);
    } catch (const prebyte::DiagnosticError&) {
    } catch (const std::exception&) {
    }

    return 0;
}
