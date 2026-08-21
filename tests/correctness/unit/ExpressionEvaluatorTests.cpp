#include "TestHarness.h"

#include "runtime/BuiltinRegistry.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/LuaExpressionEngine.h"
#include "support/Diagnostic.h"
#include "template/lexer/TemplateLexer.h"
#include "template/parser/TemplateParser.h"

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

prebyte::Value evaluate_expression(const std::string& source, prebyte::RenderSession& session,
                                   prebyte::EffectiveSettings settings = {}) {
    const std::string wrapped = "{{ " + source + " }}";
    prebyte::TemplateLexer lexer(wrapped, "inline");
    prebyte::TemplateParser parser(lexer.lex());
    const auto document = parser.parse_document();
    const prebyte::InterpolationNode* interpolation = nullptr;
    for (const auto& child : document->children) {
        if (child->kind == prebyte::TemplateNodeKind::Interpolation) {
            interpolation = static_cast<const prebyte::InterpolationNode*>(child.get());
            break;
        }
    }
    if (interpolation == nullptr || interpolation->expression == nullptr) {
        throw std::runtime_error("Expected interpolation expression in: " + source);
    }
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    return evaluator.evaluate(*interpolation->expression, settings, session, "inline");
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

TEST_CASE(ExpressionEvaluator_evaluates_literals_member_access_and_index) {
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");
    session.variables.set("user.name", "Ada");
    session.variables.set_value("items", prebyte::Value::list(prebyte::Data::Array{
        prebyte::Data("Ada"),
        prebyte::Data("Grace"),
    }));

    REQUIRE(evaluate_expression("(true)", session).to_bool());
    REQUIRE_EQ(evaluate_expression("name", session).to_string(), std::string("Ada"));
    REQUIRE_EQ(evaluate_expression("user.name", session).to_string(), std::string("Ada"));
    REQUIRE_EQ(evaluate_expression("items[0]", session).to_string(), std::string("Ada"));
    REQUIRE_EQ(evaluate_expression("len(items)", session).to_string(), std::string("2"));
}

TEST_CASE(ExpressionEvaluator_evaluates_args_index_and_grouped_expression) {
    prebyte::RenderSession session;
    session.args = {"first", "second"};

    REQUIRE_EQ(evaluate_expression("ARGS[0]", session).to_string(), std::string("first"));
    REQUIRE(evaluate_expression("(true)", session).to_bool());
}

TEST_CASE(ExpressionEvaluator_evaluates_comparisons_equality_and_in) {
    prebyte::RenderSession session;
    session.variables.set("count", "2");
    session.variables.set("label", "Ada");
    session.variables.set_value("items", prebyte::Value::list(prebyte::Data::Array{
        prebyte::Data("Ada"),
        prebyte::Data("Grace"),
    }));
    session.variables.set_value("user", prebyte::Value::object(prebyte::Data::Map{
        {"name", prebyte::Data("Ada")},
    }));

    REQUIRE(evaluate_expression("count == 2", session).to_bool());
    REQUIRE(evaluate_expression("count != 3", session).to_bool());
    REQUIRE(evaluate_expression("count < 3", session).to_bool());
    REQUIRE(evaluate_expression("count <= 2", session).to_bool());
    REQUIRE(evaluate_expression("count > 1", session).to_bool());
    REQUIRE(evaluate_expression("count >= 2", session).to_bool());
    REQUIRE(evaluate_expression("\"a\" in label", session).to_bool());
    REQUIRE(evaluate_expression("\"Ada\" in items", session).to_bool());
    REQUIRE(evaluate_expression("\"name\" in user", session).to_bool());
}

TEST_CASE(ExpressionEvaluator_evaluates_unary_not_and_filters) {
    prebyte::RenderSession session;
    session.variables.set("name", " ada ");

    REQUIRE(evaluate_expression("!false", session).to_bool());
    REQUIRE_EQ(evaluate_expression("name | trim | upper", session).to_string(), std::string("ADA"));
}

TEST_CASE(ExpressionEvaluator_evaluates_lua_call_expression) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    prebyte::LuaCallExpr expression("return 7");
    REQUIRE_EQ(evaluator.evaluate(expression, settings, session, "inline").to_string(), std::string("7"));
}

TEST_CASE(ExpressionEvaluator_rejects_structured_comparisons_and_invalid_args) {
    prebyte::RenderSession session;
    session.variables.set_value("left", prebyte::Value::object(prebyte::Data::Map{{"a", prebyte::Data(1)}}));
    session.variables.set_value("right", prebyte::Value::object(prebyte::Data::Map{{"a", prebyte::Data(2)}}));

    REQUIRE_THROWS_AS(evaluate_expression("left == right", session), prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(evaluate_expression("ARGS", session), prebyte::DiagnosticError);
}
