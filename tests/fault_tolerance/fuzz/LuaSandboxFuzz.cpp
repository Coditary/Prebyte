#include "app/AppRunner.h"
#include "app/Command.h"
#include "support/FuzzRuntimeReset.h"
#include "support/Diagnostic.h"

#include <cstddef>
#include <cstdint>
#include <fuzzer/FuzzedDataProvider.h>
#include <string>
#include <vector>

namespace {

constexpr const char* kEscapeSnippets[] = {
    "return os.execute('id')",
    "return require('os')",
    "return loadfile('secret.txt')",
    "return dofile('secret.txt')",
    "return io.open('/etc/passwd')",
    "return debug.getinfo(1)",
    "return package.loaded",
    "return getmetatable(_G)",
    "local proxy = setmetatable({}, {__index = os}); return proxy.execute('id')",
    "local chunk = load('return os.execute(\"id\")'); return chunk()",
    "return rawget(_G, 'package')",
    "return (_G)._G.os",
};

constexpr const char* kSafeSnippets[] = {
    "return load('return 41')()",
    "return string.upper('ada')",
    "return table.concat({'a', 'b'}, '-')",
    "return math.max(2, 5)",
    "return os == nil",
    "return require == nil",
};

std::string inline_lua(const std::string& lua_source) {
    return std::string("{{ lua \"") + lua_source + "\" }}";
}

std::string block_lua(const std::string& lua_source) {
    return std::string("{{ lua:block }}") + lua_source + "{{ endlua }}";
}

std::string if_lua_condition(const std::string& lua_source) {
    return std::string("{{ if lua:block }}") + lua_source + "{{ endlua }}yes{{ else }}no{{ endif }}";
}

std::string function_lua(const std::string& lua_source) {
    return std::string("{{ fn probe() lua:block }}") + lua_source + "{{ endfn }}{{ probe() }}";
}

void run_template(const std::string& template_source, const std::vector<std::string>& rule_args) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = template_source;
    command.rule_args = rule_args;

    prebyte::AppRunner runner;
    (void)runner.execute(command);
}

std::vector<std::string> limit_rules(FuzzedDataProvider& provider) {
    std::vector<std::string> rule_args;
    switch (provider.ConsumeIntegralInRange(0, 3)) {
    case 1:
        rule_args.push_back("lua_instruction_limit=10");
        break;
    case 2:
        rule_args.push_back("lua_memory_limit_bytes=4096");
        break;
    case 3:
        rule_args.push_back("max_render_time_ms=0");
        break;
    case 0:
    default:
        break;
    }
    return rule_args;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        return 0;
    }

    FuzzedDataProvider provider(data, size);
    fuzz_reset_runtime_state();

    const int route = provider.ConsumeIntegralInRange(0, 4);
    const int escape_index = provider.ConsumeIntegralInRange(0, static_cast<int>(sizeof(kEscapeSnippets) / sizeof(kEscapeSnippets[0])) - 1);
    const int safe_index = provider.ConsumeIntegralInRange(0, static_cast<int>(sizeof(kSafeSnippets) / sizeof(kSafeSnippets[0])) - 1);
    const std::vector<std::string> rule_args = limit_rules(provider);

    try {
        switch (route) {
        case 0:
            run_template(block_lua(kEscapeSnippets[escape_index]), rule_args);
            break;
        case 1:
            run_template(if_lua_condition(kEscapeSnippets[escape_index]), rule_args);
            break;
        case 2:
            run_template(function_lua(kEscapeSnippets[escape_index]), rule_args);
            break;
        case 3:
            run_template(inline_lua(kSafeSnippets[safe_index]), rule_args);
            break;
        case 4:
        default:
            run_template(block_lua(kSafeSnippets[safe_index]), rule_args);
            break;
        }
    } catch (const prebyte::DiagnosticError&) {
    } catch (const std::exception&) {
    }

    return 0;
}
