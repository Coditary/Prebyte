#include "TestHarness.h"

#include "config/RuleResolver.h"

TEST_CASE(RuleResolver_apply_global_rules) {
    prebyte::SettingsData settings;
    settings.rules["strict_variables"] = "true";
    settings.rules["case_sensitive_variables"] = "false";
    settings.rules["lua_instruction_limit"] = "5000";
    settings.rules["lua_memory_limit_bytes"] = "1048576";

    prebyte::RuleResolver resolver;
    const prebyte::ResolvedConfiguration configuration = resolver.resolve(settings, {}, {}, false);

    REQUIRE(configuration.base_settings.strict_variables);
    REQUIRE(!configuration.base_settings.case_sensitive_variables);
    REQUIRE_EQ(configuration.base_settings.lua_instruction_limit, static_cast<std::size_t>(5000));
    REQUIRE_EQ(configuration.base_settings.lua_memory_limit_bytes, static_cast<std::size_t>(1048576));
}

TEST_CASE(RuleResolver_apply_extension_rule_for_file) {
    prebyte::SettingsData settings;
    settings.file_rules.push_back(prebyte::FileRule{prebyte::RuleMatchKind::Extension, ".md", "default_variable_value", "Fallback"});

    prebyte::RuleResolver resolver;
    const prebyte::ResolvedConfiguration configuration = resolver.resolve(settings, {}, {}, false);
    const prebyte::EffectiveSettings effective = resolver.resolve_for_file(configuration, "README.md");

    REQUIRE(effective.default_variable_value.has_value());
    REQUIRE_EQ(*effective.default_variable_value, std::string("Fallback"));
}
