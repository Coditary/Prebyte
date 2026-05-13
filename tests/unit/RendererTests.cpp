#include "TestHarness.h"

#include "config/RuleResolver.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/IncludeResolver.h"
#include "runtime/Renderer.h"
#include "support/Diagnostic.h"

TEST_CASE(Renderer_replace_variable) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::ResolvedConfiguration configuration;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.configuration = configuration;
    session.variables.set("name", "Ada");

    const std::string output = renderer.render_source("Hello {{ name }}", settings, "inline", session);
    REQUIRE_EQ(output, std::string("Hello Ada"));
}

TEST_CASE(Renderer_strict_missing_variable_fails) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.strict_variables = true;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(renderer.render_source("Hello {{ missing }}", settings, "inline", session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_execute_lua_expression_and_keep_lazy_runtime) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");

    REQUIRE(!session.lua_runtime);
    REQUIRE_EQ(renderer.render_source("{{ lua \"return upper(name)\" }}", settings, "inline", session), std::string("ADA"));
    REQUIRE(session.lua_runtime != nullptr);
}

TEST_CASE(Renderer_execute_registered_lua_lower_helper) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");

    REQUIRE_EQ(renderer.render_source("{{ lua \"return lower(upper(name))\" }}", settings, "inline", session), std::string("ada"));
}

TEST_CASE(Renderer_execute_registered_lua_text_helpers) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada Lovelace");
    session.variables.set("padded", "  Ada Lovelace  ");

    REQUIRE_EQ(renderer.render_source("{{ lua \"return trim(padded)\" }}", settings, "inline", session), std::string("Ada Lovelace"));
    REQUIRE_EQ(
        renderer.render_source("{{ if lua(\"return starts_with(name, 'Ada') and ends_with(name, 'lace')\") }}ok{{ else }}bad{{ endif }}",
                               settings, "inline", session),
        std::string("ok"));
}

TEST_CASE(Renderer_fail_when_lua_helper_called_with_bad_arity) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        renderer.render_source("{{ lua \"return starts_with('Ada')\" }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_native_path_without_lua_keeps_runtime_null) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");

    REQUIRE_EQ(renderer.render_source("Hello {{ name }}", settings, "inline", session), std::string("Hello Ada"));
    REQUIRE(!session.lua_runtime);
}

TEST_CASE(Renderer_isolate_lua_globals_between_cached_executions) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    const std::string source =
        "{{ lua \"counter = (counter or 0) + 1; return counter\" }} "
        "{{ lua \"counter = (counter or 0) + 1; return counter\" }}";

    REQUIRE_EQ(renderer.render_source(source, settings, "inline", session), std::string("1 1"));
}

TEST_CASE(Renderer_fail_when_lua_exceeds_memory_limit) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        renderer.render_source("{{ lua \"return string.rep('x', 8388608)\" }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_fail_when_lua_exceeds_configured_memory_limit) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.lua_memory_limit_bytes = 1024 * 1024;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        renderer.render_source("{{ lua \"return string.rep('x', 2097152)\" }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_fail_when_lua_exceeds_configured_instruction_limit) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.lua_instruction_limit = 10;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        renderer.render_source("{{ lua \"local sum = 0 for i = 1, 1000 do sum = sum + i end return sum\" }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_replace_args_index) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.args = {"first", "second"};

    const std::string output = renderer.render_source("{{ ARGS[1] }}", settings, "inline", session);
    REQUIRE_EQ(output, std::string("second"));
}

TEST_CASE(Renderer_fail_on_bare_args_identifier) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.args = {"first"};

    REQUIRE_THROWS_AS(renderer.render_source("{{ ARGS }}", settings, "inline", session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_expose_args_to_lua) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.args = {"zero", "one"};

    REQUIRE_EQ(renderer.render_source("{{ lua \"return ARGS[0]\" }}", settings, "inline", session), std::string("zero"));
}
