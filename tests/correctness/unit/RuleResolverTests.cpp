#include "TestHarness.h"

#include "config/RuleResolver.h"
#include "support/Diagnostic.h"

TEST_CASE(RuleResolver_apply_global_rules) {
    prebyte::SettingsData settings;
    settings.rules["strict_variables"] = "true";
    settings.rules["case_sensitive_variables"] = "false";
    settings.rules["default_variable_value"] = "Fallback";
    settings.rules["variable_prefix"] = "<<";
    settings.rules["variable_suffix"] = ">>";
    settings.rules["max_variable_length"] = "12";
    settings.rules["replace_tabs"] = "true";
    settings.rules["tab_size"] = "8";
    settings.rules["trim"] = "true";
    settings.rules["allow_includes"] = "false";
    settings.rules["include_path"] = "legacy";
    settings.rules["output_encoding"] = "utf-16";
    settings.rules["allow_env"] = "true";
    settings.rules["forbidden_env_vars"] = "HOME, SECRET ,TOKEN";
    settings.rules["error_on_false_input"] = "false";
    settings.rules["lua_instruction_limit"] = "5000";
    settings.rules["lua_memory_limit_bytes"] = "1048576";
    settings.rules["max_include_depth"] = "3";
    settings.rules["max_render_time_ms"] = "250";
    settings.rules["max_output_size_bytes"] = "4096";
    settings.rules["max_loop_iteration"] = "7";
    settings.rules["debug"] = "true";

    prebyte::RuleResolver resolver;
    const prebyte::ResolvedConfiguration configuration = resolver.resolve(settings, {}, {}, {}, false);

    REQUIRE(configuration.base_settings.strict_variables);
    REQUIRE(!configuration.base_settings.case_sensitive_variables);
    REQUIRE(configuration.base_settings.default_variable_value.has_value());
    REQUIRE_EQ(*configuration.base_settings.default_variable_value, std::string("Fallback"));
    REQUIRE_EQ(configuration.base_settings.variable_prefix, std::string("<<"));
    REQUIRE_EQ(configuration.base_settings.variable_suffix, std::string(">>"));
    REQUIRE(configuration.base_settings.has_max_variable_length);
    REQUIRE_EQ(configuration.base_settings.max_variable_length, static_cast<std::size_t>(12));
    REQUIRE(configuration.base_settings.replace_tabs);
    REQUIRE_EQ(configuration.base_settings.tab_size, static_cast<std::size_t>(8));
    REQUIRE(configuration.base_settings.trim);
    REQUIRE(!configuration.base_settings.allow_includes);
    REQUIRE_EQ(configuration.base_settings.include_path.string(), std::string("legacy"));
    REQUIRE_EQ(configuration.base_settings.output_encoding, std::string("utf-16"));
    REQUIRE(configuration.base_settings.allow_env);
    REQUIRE(configuration.base_settings.forbidden_env_vars.contains("HOME"));
    REQUIRE(configuration.base_settings.forbidden_env_vars.contains("SECRET"));
    REQUIRE(configuration.base_settings.forbidden_env_vars.contains("TOKEN"));
    REQUIRE(!configuration.base_settings.error_on_false_input);
    REQUIRE_EQ(configuration.base_settings.lua_instruction_limit, static_cast<std::size_t>(5000));
    REQUIRE_EQ(configuration.base_settings.lua_memory_limit_bytes, static_cast<std::size_t>(1048576));
    REQUIRE_EQ(configuration.base_settings.max_include_depth, static_cast<std::size_t>(3));
    REQUIRE_EQ(configuration.base_settings.max_render_time_ms, static_cast<std::size_t>(250));
    REQUIRE_EQ(configuration.base_settings.max_output_size_bytes, static_cast<std::size_t>(4096));
    REQUIRE_EQ(configuration.base_settings.max_loop_iteration, static_cast<std::size_t>(7));
    REQUIRE(configuration.base_settings.debug);
}

TEST_CASE(RuleResolver_apply_extension_rule_for_file) {
    prebyte::SettingsData settings;
    settings.file_rules.push_back(prebyte::FileRule{prebyte::RuleMatchKind::Extension, ".md", "default_variable_value", "Fallback"});

    prebyte::RuleResolver resolver;
    const prebyte::ResolvedConfiguration configuration = resolver.resolve(settings, {}, {}, {}, false);
    const prebyte::EffectiveSettings effective = resolver.resolve_for_file(configuration, "README.md");

    REQUIRE(effective.default_variable_value.has_value());
    REQUIRE_EQ(*effective.default_variable_value, std::string("Fallback"));
}

TEST_CASE(RuleResolver_merge_cli_and_settings_include_paths) {
    prebyte::SettingsData settings;
    settings.include_paths = {"settings-a", "settings-b"};
    settings.rules["include_path"] = "legacy";

    prebyte::RuleResolver resolver;
    const prebyte::ResolvedConfiguration configuration = resolver.resolve(settings, {}, {}, {"cli-a", "cli-b"}, false);

    REQUIRE_EQ(configuration.base_settings.include_paths.size(), static_cast<std::size_t>(4));
    REQUIRE_EQ(configuration.base_settings.include_paths[0].string(), std::string("cli-a"));
    REQUIRE_EQ(configuration.base_settings.include_paths[1].string(), std::string("cli-b"));
    REQUIRE_EQ(configuration.base_settings.include_paths[2].string(), std::string("settings-a"));
    REQUIRE_EQ(configuration.base_settings.include_paths[3].string(), std::string("settings-b"));
    REQUIRE_EQ(configuration.base_settings.include_path.string(), std::string("legacy"));
}

TEST_CASE(RuleResolver_cli_debug_enables_debug_rule) {
    prebyte::SettingsData settings;

    prebyte::RuleResolver resolver;
    const prebyte::ResolvedConfiguration configuration = resolver.resolve(settings, {}, {}, {}, true);

    REQUIRE(configuration.base_settings.debug);
    REQUIRE_EQ(configuration.global_rules.at("debug"), std::string("true"));
}

TEST_CASE(RuleResolver_reject_invalid_new_rule_values) {
    prebyte::SettingsData settings;
    settings.rules["forbidden_env_vars"] = "HOME, SECRET";
    settings.rules["max_include_depth"] = "oops";

    prebyte::RuleResolver resolver;
    REQUIRE_THROWS_AS(resolver.resolve(settings, {}, {}, {}, false), prebyte::DiagnosticError);
}

TEST_CASE(RuleResolver_defaults_error_on_false_input_to_false_and_rejects_unknown_output_encoding) {
    prebyte::RuleResolver resolver;
    const prebyte::ResolvedConfiguration configuration = resolver.resolve(prebyte::SettingsData{}, {}, {}, {}, false);
    REQUIRE(!configuration.base_settings.error_on_false_input);

    prebyte::SettingsData bad_settings;
    bad_settings.rules["output_encoding"] = "latin-1";
    REQUIRE_THROWS_AS(resolver.resolve(bad_settings, {}, {}, {}, false), prebyte::DiagnosticError);
}
