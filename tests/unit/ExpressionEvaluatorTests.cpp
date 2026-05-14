#include "TestHarness.h"

#include "runtime/BuiltinRegistry.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/LuaExpressionEngine.h"
#include "support/Diagnostic.h"

#include <chrono>

namespace {

prebyte::CompiledProgram compile_program(const std::string& source) {
    prebyte::CompiledTemplateCompiler compiler;
    prebyte::CompiledTemplateSerializer serializer;
    return serializer.deserialize(serializer.serialize(
        compiler.compile_source(source, "inline", "inline", prebyte::EffectiveSettings{})));
}

void register_functions(prebyte::RenderSession& session, const prebyte::CompiledProgram& program) {
    for (const auto& compiled : program.functions) {
        prebyte::RenderSession::FunctionDefinition function;
        function.kind = compiled.kind == prebyte::CompiledFunction::Kind::Lua
            ? prebyte::RenderSession::FunctionDefinition::Kind::Lua
            : prebyte::RenderSession::FunctionDefinition::Kind::Template;
        function.parameters = compiled.parameters;
        function.program = &program;
        function.body_range = compiled.body_range;
        function.lua_source = compiled.lua_source;
        function.definition_file = compiled.definition_file;
        function.definition_span = compiled.span;
        session.set_function(compiled.name, std::move(function));
    }
}

}

TEST_CASE(ExpressionEvaluator_evaluate_template_function_call_directly) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    const prebyte::CompiledProgram program = compile_program("{{ fn greet(name) }}Hello {{ name }}{{ endfn }}");
    register_functions(session, program);

    std::vector<std::unique_ptr<prebyte::ExpressionNode>> arguments;
    arguments.push_back(std::make_unique<prebyte::StringExpr>("Ada"));
    prebyte::FunctionCallExpr expression("greet", std::move(arguments));

    const prebyte::Value value = evaluator.evaluate(expression, settings, session, "inline");
    REQUIRE_EQ(value.to_string(), std::string("Hello Ada"));
}

TEST_CASE(ExpressionEvaluator_evaluate_lua_function_call_directly) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    const prebyte::CompiledProgram program = compile_program(
        "{{ fn user() lua:block }}return { name = \"Ada\", active = true }{{ endfn }}");
    register_functions(session, program);

    prebyte::FunctionCallExpr expression("user", {});

    const prebyte::Value value = evaluator.evaluate(expression, settings, session, "inline");
    REQUIRE(value.is_object());
    REQUIRE_EQ(value.member("name")->to_string(), std::string("Ada"));
    REQUIRE(value.member("active")->to_bool());
}

TEST_CASE(ExpressionEvaluator_fail_on_unknown_function_call) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    prebyte::FunctionCallExpr expression("missing", {});

    REQUIRE_THROWS_AS(evaluator.evaluate(expression, settings, session, "inline"), prebyte::DiagnosticError);
}

TEST_CASE(ExpressionEvaluator_short_circuit_boolean_operators) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("present", "yes");

    prebyte::BinaryExpr and_expression(
        std::make_unique<prebyte::BoolExpr>(false),
        "&&",
        std::make_unique<prebyte::IdentifierExpr>("missing"));
    REQUIRE_EQ(evaluator.evaluate(and_expression, settings, session, "inline").to_string(), std::string("false"));

    settings.strict_variables = true;
    prebyte::BinaryExpr or_expression(
        std::make_unique<prebyte::BoolExpr>(true),
        "||",
        std::make_unique<prebyte::IdentifierExpr>("missing"));
    REQUIRE_EQ(evaluator.evaluate(or_expression, settings, session, "inline").to_string(), std::string("true"));
}

TEST_CASE(ExpressionEvaluator_enforces_direct_render_time_limit) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::EffectiveSettings settings;
    settings.max_render_time_ms = 0;
    prebyte::RenderSession session;
    session.start_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);

    prebyte::IdentifierExpr expression("name");
    REQUIRE_THROWS_AS(evaluator.evaluate(expression, settings, session, "inline"), prebyte::DiagnosticError);
}

TEST_CASE(LuaExpressionEngine_rejects_non_lua_expression) {
    prebyte::LuaExpressionEngine evaluator;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    prebyte::StringExpr expression("Ada");
    REQUIRE_THROWS_AS(evaluator.evaluate(expression, settings, session, "inline"), prebyte::DiagnosticError);
}

TEST_CASE(LuaExpressionEngine_initializes_runtime_and_evaluates) {
    prebyte::LuaExpressionEngine evaluator;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    prebyte::LuaCallExpr expression("return 42");
    const prebyte::Value value = evaluator.evaluate(expression, settings, session, "inline");

    REQUIRE(session.lua_runtime != nullptr);
    REQUIRE_EQ(value.to_string(), std::string("42"));
}
