#include "TestHarness.h"

#include <filesystem>

#include "PrebyteEngine.h"
#include "app/AppRunner.h"
#include "app/Command.h"
#include "support/Diagnostic.h"

TEST_CASE(AppRunner_render_simple_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/render_simple/input.txt";
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Hello Ada\n"));
}

TEST_CASE(AppRunner_render_include_and_if_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/render_include_if/input.txt";
    command.define_args = {"name=Ada", "enabled=true"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Header for Ada\n\nEnabled\nFooter\n"));
}

TEST_CASE(AppRunner_render_settings_profile_merge_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/settings_profile_merge/input.txt";
    command.settings_path = "tests/fixtures/settings_profile_merge/settings.yaml";
    command.profile_names = {"friendly"};
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Hi Ada\n"));
}

TEST_CASE(AppRunner_apply_file_rule_to_include_extension) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/file_rule_include/main.txt";
    command.rule_args = {".md::default_variable_value=Fallback"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Start\nValue=Fallback\n\nEnd\n"));
}

TEST_CASE(AppRunner_strict_variable_failure) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/render_simple/input.txt";
    command.rule_args = {"strict_variables=true"};

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_render_lua_inline_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/lua_inline/input.txt";
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("ADA\n"));
}

TEST_CASE(AppRunner_render_lua_block_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/lua_block/input.txt";
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Hello Ada\n"));
}

TEST_CASE(AppRunner_render_mixed_native_lua_condition_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/lua_condition/input.txt";
    command.define_args = {"enabled=true", "name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("ok\n"));
}

TEST_CASE(AppRunner_native_false_string_is_false) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ if disabled }}bad{{ else }}ok{{ endif }}";
    command.define_args = {"disabled=false"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("ok"));
}

TEST_CASE(AppRunner_benchmark_output_includes_lua_cache_stats) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ lua \"counter = (counter or 0) + 1; return counter\" }} {{ lua \"counter = (counter or 0) + 1; return counter\" }}";
    command.benchmark = true;

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("1 1\n[benchmark] ") == 0);
    REQUIRE(output.find("lua_cache_hits=1") != std::string::npos);
    REQUIRE(output.find("lua_cache_misses=1") != std::string::npos);
}

TEST_CASE(AppRunner_render_if_lua_block_condition) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ if lua:block }}\nlocal allowed = enabled == 'true'\nreturn allowed\n{{ endlua }}ok{{ else }}no{{ endif }}";
    command.define_args = {"enabled=true"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("ok"));
}

TEST_CASE(AppRunner_render_elseif_lua_block_condition) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ if disabled == true }}no{{ elseif lua:block }}\nlocal allowed = enabled == 'true'\nreturn allowed\n{{ endlua }}ok{{ else }}fallback{{ endif }}";
    command.define_args = {"disabled=false", "enabled=true"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("ok"));
}

TEST_CASE(AppRunner_render_lua_respects_configured_memory_limit) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ lua \"return string.rep('x', 2097152)\" }}";
    command.rule_args = {"lua_memory_limit_bytes=1048576"};

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_render_lua_respects_configured_instruction_limit) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ lua \"local sum = 0 for i = 1, 1000 do sum = sum + i end return sum\" }}";
    command.rule_args = {"lua_instruction_limit=10"};

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_explain_lua_includes_helpers_and_limits) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Explain;
    command.explain_topic = "lua";

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("upper(value)") != std::string::npos);
    REQUIRE(output.find("starts_with(value, prefix)") != std::string::npos);
    REQUIRE(output.find("lua_instruction_limit=100000") != std::string::npos);
    REQUIRE(output.find("lua_memory_limit_bytes=4194304") != std::string::npos);
}

TEST_CASE(AppRunner_help_includes_lua_helpers_and_limits) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Help;

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("Lua helpers:") != std::string::npos);
    REQUIRE(output.find("ends_with(value, suffix)") != std::string::npos);
    REQUIRE(output.find("lua_instruction_limit=100000") != std::string::npos);
}

TEST_CASE(AppRunner_list_rules_applies_settings_profiles_and_cli_overrides) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::ListRules;
    command.settings_path = "tests/fixtures/settings_profile_merge/settings.yaml";
    command.profile_names = {"friendly"};
    command.rule_args = {"trim=true", ".md::default_variable_value=Fallback"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("trim=true") != std::string::npos);
    REQUIRE(output.find("extension:.md::default_variable_value=Fallback") != std::string::npos);
}

TEST_CASE(AppRunner_list_vars_applies_settings_profiles_and_cli_defines) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::ListVars;
    command.settings_path = "tests/fixtures/settings_profile_merge/settings.yaml";
    command.profile_names = {"friendly"};
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("greeting=Hi") != std::string::npos);
    REQUIRE(output.find("name=Ada") != std::string::npos);
}

TEST_CASE(AppRunner_list_profiles_shows_available_profile_names) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::ListProfiles;
    command.settings_path = "tests/fixtures/settings_profile_merge/settings.yaml";

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("friendly") != std::string::npos);
    REQUIRE(output.find("strict") != std::string::npos);
}

TEST_CASE(AppRunner_list_ignores_merges_settings_profiles_and_cli) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::ListIgnores;
    command.settings_path = "tests/fixtures/settings_profile_merge/settings.yaml";
    command.profile_names = {"friendly"};
    command.ignore_names = {"cli_only"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("base_ignore") != std::string::npos);
    REQUIRE(output.find("friendly_ignore") != std::string::npos);
    REQUIRE(output.find("cli_only") != std::string::npos);
}

TEST_CASE(AppRunner_render_args_are_available_in_native_and_lua) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ ARGS[0] }} {{ lua \"return ARGS[2]\" }}";
    command.render_args = {"alpha", "beta", "gamma"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE_EQ(output, std::string("alpha gamma"));
}

TEST_CASE(AppRunner_explain_args_describes_render_args) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Explain;
    command.explain_topic = "ARGS";

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("ARGS[index]") != std::string::npos);
    REQUIRE(output.find("prebyte template.txt foo bar") != std::string::npos);
}

TEST_CASE(AppRunner_help_mentions_list_profiles_and_render_args) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Help;

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("list rules|vars|profiles|ignore|ignores") != std::string::npos);
    REQUIRE(output.find("ARGS[index]") != std::string::npos);
}

TEST_CASE(PrebyteEngine_render_args_are_available) {
    prebyte::Prebyte engine;
    engine.add_argument("alpha");
    engine.add_argument("beta");

    const std::string output = engine.process("{{ ARGS[1] }}");
    REQUIRE_EQ(output, std::string("beta"));
}
