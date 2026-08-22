#include "TestHarness.h"

#include "app/AppRunner.h"
#include "app/Command.h"
#include "config/RuleResolver.h"
#include "io/InputBuffer.h"
#include "runtime/expression/BuiltinRegistry.h"
#include "runtime/expression/ExpressionEvaluator.h"
#include "runtime/resolution/IncludeResolver.h"
#include "runtime/render/Renderer.h"
#include "support/Diagnostic.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

struct SettingsHarness {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator{builtins};
    prebyte::Renderer renderer{rule_resolver, include_resolver, evaluator};
};

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path settings_test_root(const std::string& name) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "prebyte-settings-behavior-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

prebyte::EffectiveSettings effective_settings_from_rules(
    const std::vector<std::string>& rule_args, const std::filesystem::path& file_path = "inline.pbt") {
    prebyte::RuleResolver resolver;
    const prebyte::ResolvedConfiguration configuration =
        resolver.resolve(prebyte::SettingsData{}, rule_args, {}, {}, false);
    return resolver.resolve_for_file(configuration, file_path);
}

std::string render_with_rules(SettingsHarness& harness, const std::string& source,
                              const std::vector<std::string>& rule_args, prebyte::RenderSession session,
                              const std::filesystem::path& current_file = "inline.pbt") {
    const prebyte::EffectiveSettings settings = effective_settings_from_rules(rule_args, current_file);
    return harness.renderer.render_source(source, settings, current_file, session);
}

std::string app_runner_render_inline(const std::string& source, const std::vector<std::string>& rule_args,
                                     const std::vector<std::string>& define_args = {}) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = source;
    command.rule_args = rule_args;
    command.define_args = define_args;
    prebyte::AppRunner runner;
    return runner.execute(command);
}

void expect_app_runner_error(const std::string& source, const std::vector<std::string>& rule_args,
                             const std::vector<std::string>& define_args = {}) {
    try {
        app_runner_render_inline(source, rule_args, define_args);
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError&) {
    }
}

void expect_render_error(SettingsHarness& harness, const std::string& source,
                         const std::vector<std::string>& rule_args, prebyte::RenderSession session,
                         const std::filesystem::path& current_file = "inline.pbt") {
    REQUIRE_THROWS_AS(render_with_rules(harness, source, rule_args, session, current_file),
                      prebyte::DiagnosticError);
}

prebyte::RenderSession session_with_name(const std::string& name = "Ada") {
    prebyte::RenderSession session;
    session.variables.set("name", name);
    session.variables.set("enabled", "false");
    session.variables.set("label", "  Ada Lovelace  ");
    session.variables.set("Name", "CaseSensitive");
    return session;
}

}

TEST_CASE(SettingsBehavior_strict_variables_enabled_rejects_missing_variable) {
    SettingsHarness harness;
    expect_render_error(harness, "{{ missing }}", {"strict_variables=true"}, session_with_name());
}

TEST_CASE(SettingsBehavior_strict_variables_disabled_allows_missing_variable) {
    SettingsHarness harness;
    const std::string output = render_with_rules(harness, "{{ missing }}", {"strict_variables=false"},
                                                 session_with_name());
    REQUIRE(output.empty());
}

TEST_CASE(SettingsBehavior_default_variable_value_enabled_fills_missing_variable) {
    SettingsHarness harness;
    const std::string output = render_with_rules(
        harness, "{{ missing }}", {"strict_variables=false", "default_variable_value=Fallback"}, session_with_name());
    REQUIRE_EQ(output, std::string("Fallback"));
}

TEST_CASE(SettingsBehavior_default_variable_value_disabled_leaves_missing_variable_empty) {
    SettingsHarness harness;
    const std::string output =
        render_with_rules(harness, "{{ missing }}", {"strict_variables=false"}, session_with_name());
    REQUIRE(output.empty());
}

TEST_CASE(SettingsBehavior_case_sensitive_variables_enabled_requires_exact_name) {
    SettingsHarness harness;
    prebyte::RenderSession session;
    session.variables.set("Name", "CaseSensitive");
    REQUIRE_EQ(render_with_rules(harness, "{{ Name }}", {"case_sensitive_variables=true"}, session),
               std::string("CaseSensitive"));
    REQUIRE(render_with_rules(harness, "{{ name }}", {"case_sensitive_variables=true"}, session).empty());
}

TEST_CASE(SettingsBehavior_case_sensitive_variables_disabled_ignores_name_case) {
    SettingsHarness harness;
    prebyte::RenderSession session;
    session.variables.set("Name", "Ada");
    REQUIRE_EQ(render_with_rules(harness, "{{ name }}", {"case_sensitive_variables=false"}, session),
               std::string("Ada"));
}

TEST_CASE(SettingsBehavior_custom_variable_delimiters_enabled_change_interpolation) {
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    const std::string source = "<< name >>";
    REQUIRE_EQ(render_with_rules(harness, source,
                                 {"variable_prefix=<<", "variable_suffix=>>"}, session),
               std::string("Ada"));
}

TEST_CASE(SettingsBehavior_custom_variable_delimiters_disabled_keep_default_syntax) {
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    REQUIRE_EQ(render_with_rules(harness, "<< name >>", {}, session), std::string("<< name >>"));
    REQUIRE_EQ(render_with_rules(harness, "{{ name }}", {}, session), std::string("Ada"));
}

TEST_CASE(SettingsBehavior_trim_enabled_strips_variable_whitespace) {
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    REQUIRE_EQ(render_with_rules(harness, "{{ label }}", {"trim=true"}, session), std::string("Ada Lovelace"));
}

TEST_CASE(SettingsBehavior_trim_disabled_preserves_variable_whitespace) {
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    REQUIRE_EQ(render_with_rules(harness, "{{ label }}", {"trim=false"}, session), std::string("  Ada Lovelace  "));
}

TEST_CASE(SettingsBehavior_max_variable_length_enabled_truncates_values) {
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    REQUIRE_EQ(render_with_rules(harness, "{{ label }}", {"max_variable_length=3"}, session),
               std::string("  A"));
}

TEST_CASE(SettingsBehavior_max_variable_length_disabled_keeps_full_values) {
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    REQUIRE_EQ(render_with_rules(harness, "{{ label }}", {}, session), std::string("  Ada Lovelace  "));
}

TEST_CASE(SettingsBehavior_replace_tabs_enabled_expands_tabs_in_template) {
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    REQUIRE_EQ(render_with_rules(harness, "A\tB", {"replace_tabs=true", "tab_size=4"}, session),
               std::string("A    B"));
}

TEST_CASE(SettingsBehavior_replace_tabs_disabled_keeps_literal_tabs) {
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    REQUIRE_EQ(render_with_rules(harness, "A\tB", {"replace_tabs=false"}, session), std::string("A\tB"));
}

TEST_CASE(SettingsBehavior_allow_includes_enabled_renders_include) {
    const std::filesystem::path root = settings_test_root("allow-includes-on");
    write_file(root / "partial.pbt", "Partial {{ name }}");
    write_file(root / "main.pbt", "{{ include \"partial.pbt\" }}");

    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    const std::string output = render_with_rules(harness, "{{ include \"partial.pbt\" }}",
                                                   {"allow_includes=true", "include_path=" + root.string()}, session,
                                                   root / "main.pbt");
    REQUIRE_EQ(output, std::string("Partial Ada"));
}

TEST_CASE(SettingsBehavior_allow_includes_disabled_rejects_include) {
    const std::filesystem::path root = settings_test_root("allow-includes-off");
    write_file(root / "partial.pbt", "Partial {{ name }}");
    write_file(root / "main.pbt", "{{ include \"partial.pbt\" }}");

    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    expect_render_error(harness, "{{ include \"partial.pbt\" }}",
                        {"allow_includes=false", "include_path=" + root.string()}, session, root / "main.pbt");
}

TEST_CASE(SettingsBehavior_max_include_depth_enabled_limits_nested_includes) {
    const std::filesystem::path root = settings_test_root("include-depth-on");
    write_file(root / "level2.pbt", "deep");
    write_file(root / "level1.pbt", "{{ include \"level2.pbt\" }}");
    write_file(root / "main.pbt", "{{ include \"level1.pbt\" }}");

    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    expect_render_error(harness, "{{ include \"level1.pbt\" }}",
                        {"allow_includes=true", "include_path=" + root.string(), "max_include_depth=0"}, session,
                        root / "main.pbt");
}

TEST_CASE(SettingsBehavior_max_include_depth_disabled_allows_nested_includes) {
    const std::filesystem::path root = settings_test_root("include-depth-off");
    write_file(root / "level2.pbt", "deep");
    write_file(root / "level1.pbt", "{{ include \"level2.pbt\" }}");
    write_file(root / "main.pbt", "{{ include \"level1.pbt\" }}");

    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    const std::string output = render_with_rules(harness, "{{ include \"level1.pbt\" }}",
                                                 {"allow_includes=true", "include_path=" + root.string()}, session,
                                                 root / "main.pbt");
    REQUIRE_EQ(output, std::string("deep"));
}

TEST_CASE(SettingsBehavior_allow_env_enabled_reads_environment_variable) {
    prebyte::test::ScopedEnvironmentVariable allowed_env("PREBYTE_SETTINGS_BEHAVIOR_ENV", "Grace");

    SettingsHarness harness;
    REQUIRE_EQ(render_with_rules(harness, "{{ PREBYTE_SETTINGS_BEHAVIOR_ENV }}", {"allow_env=true"},
                                 session_with_name()),
               std::string("Grace"));
}

TEST_CASE(SettingsBehavior_allow_env_disabled_hides_environment_variable) {
    prebyte::test::ScopedEnvironmentVariable allowed_env("PREBYTE_SETTINGS_BEHAVIOR_ENV", "Grace");

    SettingsHarness harness;
    const std::string output = render_with_rules(
        harness, "{{ PREBYTE_SETTINGS_BEHAVIOR_ENV }}",
        {"allow_env=false", "strict_variables=false", "default_variable_value=Fallback"}, session_with_name());
    REQUIRE_EQ(output, std::string("Fallback"));
}

TEST_CASE(SettingsBehavior_forbidden_env_vars_enabled_blocks_listed_variable) {
    prebyte::test::ScopedEnvironmentVariable blocked_env("PREBYTE_SETTINGS_BEHAVIOR_BLOCKED", "Secret");

    SettingsHarness harness;
    expect_render_error(harness, "{{ PREBYTE_SETTINGS_BEHAVIOR_BLOCKED }}",
                        {"allow_env=true", "forbidden_env_vars=PREBYTE_SETTINGS_BEHAVIOR_BLOCKED"},
                        session_with_name());
}

TEST_CASE(SettingsBehavior_forbidden_env_vars_disabled_allows_environment_variable) {
    prebyte::test::ScopedEnvironmentVariable blocked_env("PREBYTE_SETTINGS_BEHAVIOR_BLOCKED", "Secret");

    SettingsHarness harness;
    REQUIRE_EQ(render_with_rules(harness, "{{ PREBYTE_SETTINGS_BEHAVIOR_BLOCKED }}", {"allow_env=true"},
                                 session_with_name()),
               std::string("Secret"));
}

TEST_CASE(SettingsBehavior_error_on_false_input_enabled_rejects_false_condition) {
    expect_app_runner_error("{{ if enabled }}yes{{ else }}no{{ endif }}",
                            {"error_on_false_input=true"},
                            {"enabled=false"});
}

TEST_CASE(SettingsBehavior_error_on_false_input_disabled_allows_false_condition) {
    REQUIRE_EQ(app_runner_render_inline("{{ if enabled }}yes{{ else }}no{{ endif }}",
                                        {"error_on_false_input=false"},
                                        {"enabled=false"}),
               std::string("no"));
}

TEST_CASE(SettingsBehavior_max_output_size_bytes_enabled_rejects_large_output) {
    expect_app_runner_error("Hello {{ name }}", {"max_output_size_bytes=5"});
}

TEST_CASE(SettingsBehavior_max_output_size_bytes_disabled_allows_large_output) {
    REQUIRE_EQ(app_runner_render_inline("Hello {{ name }}", {"max_output_size_bytes=100"}, {"name=Ada"}),
               std::string("Hello Ada"));
}

TEST_CASE(SettingsBehavior_max_loop_iteration_enabled_limits_nested_loops) {
    const std::string source =
        "{{ fn groups() lua:block }}return { {'A', 'B'}, {'C'} }{{ endfn }}"
        "{{ for group in groups() }}{{ for item in group }}{{ item }}{{ endfor }}|{{ endfor }}";
    expect_app_runner_error(source, {"max_loop_iteration=1"});
}

TEST_CASE(SettingsBehavior_max_loop_iteration_disabled_allows_nested_loops) {
    const std::string source =
        "{{ fn groups() lua:block }}return { {'A', 'B'}, {'C'} }{{ endfn }}"
        "{{ for group in groups() }}{{ for item in group }}{{ item }}{{ endfor }}|{{ endfor }}";
    REQUIRE_EQ(app_runner_render_inline(source, {"max_loop_iteration=100"}), std::string("AB|C|"));
}

TEST_CASE(SettingsBehavior_max_render_time_ms_enabled_interrupts_long_lua) {
    expect_app_runner_error("{{ lua:block }} while true do end return 'x' {{ endlua }}", {"max_render_time_ms=0"});
}

TEST_CASE(SettingsBehavior_max_render_time_ms_disabled_allows_short_lua) {
    REQUIRE_EQ(app_runner_render_inline("{{ lua \"return 'ok'\" }}", {"max_render_time_ms=1000"}),
               std::string("ok"));
}

TEST_CASE(SettingsBehavior_lua_instruction_limit_enabled_rejects_heavy_script) {
    const std::string source =
        "{{ lua \"local sum = 0 for i = 1, 1000 do sum = sum + i end return sum\" }}";
    expect_app_runner_error(source, {"lua_instruction_limit=10"});
}

TEST_CASE(SettingsBehavior_lua_instruction_limit_disabled_allows_heavy_script) {
    const std::string source =
        "{{ lua \"local sum = 0 for i = 1, 1000 do sum = sum + i end return sum\" }}";
    REQUIRE_EQ(app_runner_render_inline(source, {"lua_instruction_limit=100000"}), std::string("500500"));
}

TEST_CASE(SettingsBehavior_lua_memory_limit_bytes_enabled_rejects_large_allocation) {
    expect_app_runner_error("{{ lua \"return string.rep('x', 2097152)\" }}", {"lua_memory_limit_bytes=1048576"});
}

TEST_CASE(SettingsBehavior_lua_memory_limit_bytes_disabled_allows_small_allocation) {
    REQUIRE_EQ(app_runner_render_inline("{{ lua \"return string.rep('x', 8)\" }}", {"lua_memory_limit_bytes=1048576"}),
               std::string("xxxxxxxx"));
}

TEST_CASE(SettingsBehavior_output_encoding_utf16_enabled_writes_bom) {
    const std::filesystem::path root = settings_test_root("output-encoding-on");
    const std::filesystem::path output_path = root / "out.txt";

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "Hello";
    command.output_path = output_path;
    command.rule_args = {"output_encoding=utf-16"};

    prebyte::AppRunner runner;
    runner.run(command);

    const std::string bytes = std::string(prebyte::InputBuffer::from_file(output_path).view());
    REQUIRE_EQ(bytes.size(), static_cast<std::size_t>(12));
    REQUIRE_EQ(static_cast<unsigned char>(bytes[0]), 0xFFu);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[1]), 0xFEu);
}

TEST_CASE(SettingsBehavior_output_encoding_utf8_disabled_writes_plain_text) {
    const std::filesystem::path root = settings_test_root("output-encoding-off");
    const std::filesystem::path output_path = root / "out.txt";

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "Hello";
    command.output_path = output_path;
    command.rule_args = {"output_encoding=utf-8"};

    prebyte::AppRunner runner;
    runner.run(command);

    REQUIRE_EQ(std::string(prebyte::InputBuffer::from_file(output_path).view()), std::string("Hello"));
}

TEST_CASE(SettingsBehavior_file_rule_enabled_applies_extension_scoped_default) {
    const std::filesystem::path file_path = "notes.md";
    const prebyte::EffectiveSettings settings =
        effective_settings_from_rules({".md::default_variable_value=Fallback"}, file_path);
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    REQUIRE_EQ(harness.renderer.render_source("{{ missing }}", settings, file_path, session),
               std::string("Fallback"));
}

TEST_CASE(SettingsBehavior_file_rule_disabled_keeps_global_default_empty) {
    const std::filesystem::path file_path = "notes.md";
    SettingsHarness harness;
    const std::string output = render_with_rules(
        harness, "{{ missing }}", {"strict_variables=false"}, session_with_name(), file_path);
    REQUIRE(output.empty());
}

TEST_CASE(SettingsBehavior_combination_trim_and_max_variable_length_apply_in_order) {
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    session.variables.set("label", "  Ada  ");
    REQUIRE_EQ(render_with_rules(harness, "{{ label }}", {"trim=true", "max_variable_length=2"}, session),
               std::string("Ad"));
}

TEST_CASE(SettingsBehavior_combination_default_value_and_trim_normalize_missing_variable) {
    SettingsHarness harness;
    REQUIRE_EQ(render_with_rules(harness, "{{ missing }}",
                                 {"strict_variables=false", "default_variable_value=  padded  ", "trim=true"},
                                 session_with_name()),
               std::string("padded"));
}

TEST_CASE(SettingsBehavior_combination_strict_and_case_sensitive_require_exact_missing_name) {
    SettingsHarness harness;
    prebyte::RenderSession session;
    session.variables.set("Name", "Exact");
    expect_render_error(harness, "{{ name }}", {"strict_variables=true", "case_sensitive_variables=true"}, session);
    REQUIRE_EQ(render_with_rules(harness, "{{ Name }}", {"strict_variables=false", "case_sensitive_variables=true"},
                                 session),
               std::string("Exact"));
}

TEST_CASE(SettingsBehavior_combination_allow_env_and_forbidden_env_vars_block_only_listed_names) {
    prebyte::test::ScopedEnvironmentVariable allowed_env("PREBYTE_SETTINGS_BEHAVIOR_ALLOWED", "Ada");
    prebyte::test::ScopedEnvironmentVariable blocked_env("PREBYTE_SETTINGS_BEHAVIOR_BLOCKED", "Secret");

    SettingsHarness harness;
    const std::vector<std::string> rules = {"allow_env=true", "forbidden_env_vars=PREBYTE_SETTINGS_BEHAVIOR_BLOCKED"};
    REQUIRE_EQ(render_with_rules(harness, "{{ PREBYTE_SETTINGS_BEHAVIOR_ALLOWED }}", rules, session_with_name()),
               std::string("Ada"));
    expect_render_error(harness, "{{ PREBYTE_SETTINGS_BEHAVIOR_BLOCKED }}", rules, session_with_name());
}

TEST_CASE(SettingsBehavior_combination_allow_includes_and_max_include_depth_limit_together) {
    const std::filesystem::path root = settings_test_root("include-combo");
    write_file(root / "level2.pbt", "deep");
    write_file(root / "level1.pbt", "{{ include \"level2.pbt\" }}");
    write_file(root / "main.pbt", "{{ include \"level1.pbt\" }}");

    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    const std::vector<std::string> strict_rules = {
        "allow_includes=true",
        "include_path=" + root.string(),
        "max_include_depth=1",
    };
    expect_render_error(harness, "{{ include \"level1.pbt\" }}", strict_rules, session, root / "main.pbt");

    const std::vector<std::string> relaxed_rules = {
        "allow_includes=true",
        "include_path=" + root.string(),
        "max_include_depth=2",
    };
    REQUIRE_EQ(render_with_rules(harness, "{{ include \"level1.pbt\" }}", relaxed_rules, session, root / "main.pbt"),
               std::string("deep"));
}

TEST_CASE(SettingsBehavior_combination_replace_tabs_and_tab_size_control_expansion_width) {
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    REQUIRE_EQ(render_with_rules(harness, "X\tY", {"replace_tabs=true", "tab_size=2"}, session),
               std::string("X  Y"));
    REQUIRE_EQ(render_with_rules(harness, "X\tY", {"replace_tabs=true", "tab_size=6"}, session),
               std::string("X      Y"));
}

TEST_CASE(SettingsBehavior_combination_custom_delimiters_require_both_prefix_and_suffix) {
    SettingsHarness harness;
    prebyte::RenderSession session = session_with_name();
    REQUIRE_EQ(render_with_rules(harness, "{{ name }}", {"variable_prefix=<<", "variable_suffix=>>"}, session),
               std::string("{{ name }}"));
    REQUIRE_EQ(render_with_rules(harness, "<< name >>", {"variable_prefix=<<", "variable_suffix=>>"}, session),
               std::string("Ada"));
}

TEST_CASE(SettingsBehavior_combination_error_on_false_input_and_default_value_are_independent) {
    const std::vector<std::string> defines = {"enabled=false"};
    REQUIRE_EQ(app_runner_render_inline("{{ if enabled }}yes{{ else }}{{ missing }}{{ endif }}",
                                        {"error_on_false_input=false", "strict_variables=false",
                                         "default_variable_value=Fallback"},
                                        defines),
               std::string("Fallback"));
    expect_app_runner_error("{{ if enabled }}yes{{ else }}{{ missing }}{{ endif }}",
                            {"error_on_false_input=true", "strict_variables=false",
                             "default_variable_value=Fallback"},
                            defines);
}
