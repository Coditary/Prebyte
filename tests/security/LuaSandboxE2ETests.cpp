#include "TestHarness.h"

#include "app/AppRunner.h"
#include "app/Command.h"
#include "support/Diagnostic.h"

namespace {

std::string render_inline_lua(const std::string& template_source, const std::vector<std::string>& rule_args = {}) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = template_source;
    command.rule_args = rule_args;
    prebyte::AppRunner runner;
    return runner.execute(command);
}

void expect_render_error(const std::string& template_source, const std::vector<std::string>& rule_args = {}) {
    try {
        render_inline_lua(template_source, rule_args);
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError&) {
    }
}

std::string inline_lua(const std::string& lua_source) {
    return std::string("{{ lua \"") + lua_source + "\" }}";
}

std::string block_lua(const std::string& lua_source) {
    return std::string("{{ lua:block }}") + lua_source + "{{ endlua }}";
}

}

TEST_CASE(LuaSandboxE2E_removed_globals_are_nil_in_inline_lua) {
    REQUIRE_EQ(render_inline_lua(inline_lua("return os == nil")), std::string("true"));
    REQUIRE_EQ(render_inline_lua(inline_lua("return io == nil")), std::string("true"));
    REQUIRE_EQ(render_inline_lua(inline_lua("return debug == nil")), std::string("true"));
    REQUIRE_EQ(render_inline_lua(inline_lua("return package == nil")), std::string("true"));
    REQUIRE_EQ(render_inline_lua(inline_lua("return require == nil")), std::string("true"));
    REQUIRE_EQ(render_inline_lua(inline_lua("return dofile == nil")), std::string("true"));
    REQUIRE_EQ(render_inline_lua(inline_lua("return loadfile == nil")), std::string("true"));
}

TEST_CASE(LuaSandboxE2E_removed_globals_are_nil_in_lua_block) {
    REQUIRE_EQ(render_inline_lua(block_lua("return os == nil")), std::string("true"));
    REQUIRE_EQ(render_inline_lua(block_lua("return io == nil")), std::string("true"));
    REQUIRE_EQ(render_inline_lua(block_lua("return require == nil")), std::string("true"));
}

TEST_CASE(LuaSandboxE2E_os_execute_require_and_loadfile_fail_in_templates) {
    expect_render_error(inline_lua("return os.execute('id')"));
    expect_render_error(inline_lua("return require('os')"));
    expect_render_error(inline_lua("return loadfile('secret.txt')"));
    expect_render_error(inline_lua("return dofile('secret.txt')"));
    expect_render_error(block_lua("return io.open('/etc/passwd')"));
    expect_render_error(block_lua("return debug.getinfo(1)"));
}

TEST_CASE(LuaSandboxE2E_metatable_escape_attempts_stay_sandboxed) {
    REQUIRE_EQ(render_inline_lua(inline_lua("return getmetatable(_G) == false")), std::string("true"));
    expect_render_error(inline_lua("local proxy = setmetatable({}, {__index = os}); return proxy.execute('id')"));
    expect_render_error(block_lua("local chunk = load('return os.execute(\"id\")'); return chunk()"));
    REQUIRE_EQ(render_inline_lua(inline_lua("return rawget(_G, 'package') == nil")), std::string("true"));
}

TEST_CASE(LuaSandboxE2E_safe_load_and_standard_library_still_work) {
    REQUIRE_EQ(render_inline_lua(inline_lua("return load('return 41')()")), std::string("41"));
    REQUIRE_EQ(render_inline_lua(block_lua("return string.upper('ada')")), std::string("ADA"));
    REQUIRE_EQ(render_inline_lua(block_lua("return table.concat({'a', 'b'}, '-')")), std::string("a-b"));
    REQUIRE_EQ(render_inline_lua(block_lua("return math.max(2, 5)")), std::string("5"));
}

TEST_CASE(LuaSandboxE2E_instruction_memory_and_time_limits_apply_via_app_runner) {
    expect_render_error(inline_lua("local sum = 0 for i = 1, 1000 do sum = sum + i end return sum"),
                        {"lua_instruction_limit=10"});
    expect_render_error(inline_lua("return string.rep('x', 2097152)"), {"lua_memory_limit_bytes=1048576"});
    expect_render_error(block_lua("while true do end return 'x'"), {"max_render_time_ms=0"});
}

TEST_CASE(LuaSandboxE2E_lua_condition_in_if_stays_sandboxed) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input =
        "{{ if lua:block }}return os == nil{{ endlua }}sandboxed{{ else }}escaped{{ endif }}";
    command.define_args = {"enabled=true"};

    prebyte::AppRunner runner;
    REQUIRE_EQ(runner.execute(command), std::string("sandboxed"));
}

TEST_CASE(LuaSandboxE2E_lua_function_definition_stays_sandboxed) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input =
        "{{ fn probe() lua:block }}return require == nil and os == nil{{ endfn }}"
        "{{ if probe() }}ok{{ else }}bad{{ endif }}";

    prebyte::AppRunner runner;
    REQUIRE_EQ(runner.execute(command), std::string("ok"));
}
