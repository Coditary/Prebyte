#include "TestHarness.h"

#include "template/lexer/TemplateLexer.h"
#include "template/parser/TemplateParser.h"

TEST_CASE(TemplateParser_parse_if_document) {
    prebyte::TemplateLexer lexer("Hello {{ if enabled }}Yes{{ else }}No{{ endif }}", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(static_cast<int>(document->children[1]->kind), static_cast<int>(prebyte::TemplateNodeKind::If));
}

TEST_CASE(TemplateParser_fail_on_unexpected_else) {
    prebyte::TemplateLexer lexer("{{ else }}", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    REQUIRE_THROWS_AS(parser.parse_document(), prebyte::DiagnosticError);
}

TEST_CASE(TemplateParser_parse_lua_expression_and_call) {
    prebyte::TemplateLexer lexer("{{ lua \"return 42\" }} {{ if lua(\"return true\") }}ok{{ endif }}", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(3));
    REQUIRE_EQ(static_cast<int>(document->children[0]->kind), static_cast<int>(prebyte::TemplateNodeKind::LuaExpr));
    REQUIRE_EQ(static_cast<int>(document->children[2]->kind), static_cast<int>(prebyte::TemplateNodeKind::If));
}

TEST_CASE(TemplateParser_parse_if_lua_block_condition) {
    prebyte::TemplateLexer lexer("{{ if lua:block }}return enabled == 'true'{{ endlua }}ok{{ endif }}", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(static_cast<int>(document->children[0]->kind), static_cast<int>(prebyte::TemplateNodeKind::If));

    const auto& if_node = static_cast<const prebyte::IfNode&>(*document->children[0]);
    REQUIRE_EQ(if_node.branches.size(), static_cast<std::size_t>(1));
    REQUIRE(if_node.branches[0].condition != nullptr);
    REQUIRE_EQ(static_cast<int>(if_node.branches[0].condition->kind), static_cast<int>(prebyte::ExpressionKind::LuaCall));

    const auto& condition = static_cast<const prebyte::LuaCallExpr&>(*if_node.branches[0].condition);
    REQUIRE_EQ(condition.source, std::string("return enabled == 'true'"));
}

TEST_CASE(TemplateParser_parse_args_index_identifier) {
    prebyte::TemplateLexer lexer("{{ ARGS[0] }}", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(static_cast<int>(document->children[0]->kind), static_cast<int>(prebyte::TemplateNodeKind::Interpolation));

    const auto& interpolation = static_cast<const prebyte::InterpolationNode&>(*document->children[0]);
    REQUIRE_EQ(static_cast<int>(interpolation.expression->kind), static_cast<int>(prebyte::ExpressionKind::IndexAccess));

    const auto& index_access = static_cast<const prebyte::IndexAccessExpr&>(*interpolation.expression);
    REQUIRE(index_access.base != nullptr);
    REQUIRE(index_access.index != nullptr);
    REQUIRE_EQ(static_cast<int>(index_access.base->kind), static_cast<int>(prebyte::ExpressionKind::Identifier));
    REQUIRE_EQ(static_cast<const prebyte::IdentifierExpr&>(*index_access.base).name, std::string("ARGS"));
    REQUIRE_EQ(static_cast<int>(index_access.index->kind), static_cast<int>(prebyte::ExpressionKind::Number));
}

TEST_CASE(TemplateParser_accept_dotted_identifier_expression) {
    prebyte::TemplateLexer lexer("{{ user.name }}", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(static_cast<int>(document->children[0]->kind), static_cast<int>(prebyte::TemplateNodeKind::Interpolation));

    const auto& interpolation = static_cast<const prebyte::InterpolationNode&>(*document->children[0]);
    REQUIRE_EQ(static_cast<int>(interpolation.expression->kind), static_cast<int>(prebyte::ExpressionKind::MemberAccess));
}

TEST_CASE(TemplateParser_parse_list_index_expression) {
    prebyte::TemplateLexer lexer("{{ items[0] }}", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(1));
    const auto& interpolation = static_cast<const prebyte::InterpolationNode&>(*document->children[0]);
    REQUIRE_EQ(static_cast<int>(interpolation.expression->kind), static_cast<int>(prebyte::ExpressionKind::IndexAccess));
}

TEST_CASE(TemplateParser_parse_len_call_expression) {
    prebyte::TemplateLexer lexer("{{ len(items[0]) }}", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(1));
    const auto& interpolation = static_cast<const prebyte::InterpolationNode&>(*document->children[0]);
    REQUIRE_EQ(static_cast<int>(interpolation.expression->kind), static_cast<int>(prebyte::ExpressionKind::LenCall));

    const auto& len_call = static_cast<const prebyte::LenCallExpr&>(*interpolation.expression);
    REQUIRE(len_call.operand != nullptr);
    REQUIRE_EQ(static_cast<int>(len_call.operand->kind), static_cast<int>(prebyte::ExpressionKind::IndexAccess));
}

TEST_CASE(TemplateParser_parse_for_else_block) {
    prebyte::TemplateLexer lexer("{{ for item in items }}{{ item }}{{ else }}empty{{ endfor }}", "inline");
    prebyte::TemplateParser parser(lexer.lex(), prebyte::TemplateParserOptions{.enable_loops = true});
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(static_cast<int>(document->children[0]->kind), static_cast<int>(prebyte::TemplateNodeKind::For));

    const auto& for_node = static_cast<const prebyte::ForNode&>(*document->children[0]);
    REQUIRE_EQ(for_node.value_name, std::string("item"));
    REQUIRE(!for_node.key_name.has_value());
    REQUIRE(for_node.iterable != nullptr);
    REQUIRE_EQ(for_node.body.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(for_node.else_body.size(), static_cast<std::size_t>(1));
}

TEST_CASE(TemplateParser_reject_reserved_loop_binding_name) {
    prebyte::TemplateLexer lexer("{{ for loop in items }}{{ endfor }}", "inline");
    prebyte::TemplateParser parser(lexer.lex(), prebyte::TemplateParserOptions{.enable_loops = true});
    REQUIRE_THROWS_AS(parser.parse_document(), prebyte::DiagnosticError);
}

TEST_CASE(TemplateParser_reject_duplicate_object_loop_binding_names) {
    prebyte::TemplateLexer lexer("{{ for key, key in user }}{{ endfor }}", "inline");
    prebyte::TemplateParser parser(lexer.lex(), prebyte::TemplateParserOptions{.enable_loops = true});
    REQUIRE_THROWS_AS(parser.parse_document(), prebyte::DiagnosticError);
}

TEST_CASE(TemplateParser_parse_nested_if_and_for_document) {
    prebyte::TemplateLexer lexer(
        "{{ if groups }}{{ for group in groups }}{{ if group.featured }}x{{ elseif group.archived }}y{{ else }}z{{ endif }}{{ endfor }}{{ endif }}",
        "inline");
    prebyte::TemplateParser parser(lexer.lex(), prebyte::TemplateParserOptions{.enable_loops = true});
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(static_cast<int>(document->children[0]->kind), static_cast<int>(prebyte::TemplateNodeKind::If));

    const auto& if_node = static_cast<const prebyte::IfNode&>(*document->children[0]);
    REQUIRE_EQ(if_node.branches.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(if_node.branches[0].body.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(static_cast<int>(if_node.branches[0].body[0]->kind), static_cast<int>(prebyte::TemplateNodeKind::For));

    const auto& for_node = static_cast<const prebyte::ForNode&>(*if_node.branches[0].body[0]);
    REQUIRE_EQ(for_node.body.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(static_cast<int>(for_node.body[0]->kind), static_cast<int>(prebyte::TemplateNodeKind::If));
}

TEST_CASE(TemplateParser_parse_set_statement_with_pipe_chain) {
    prebyte::TemplateLexer lexer("{{ set title = user.name | trim | upper }}", "inline");
    prebyte::TemplateParser parser(lexer.lex(), prebyte::TemplateParserOptions{.enable_loops = true});
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(static_cast<int>(document->children[0]->kind), static_cast<int>(prebyte::TemplateNodeKind::Set));

    const auto& set_node = static_cast<const prebyte::SetNode&>(*document->children[0]);
    REQUIRE_EQ(set_node.name, std::string("title"));
    REQUIRE(set_node.expression != nullptr);
    REQUIRE_EQ(static_cast<int>(set_node.expression->kind), static_cast<int>(prebyte::ExpressionKind::FilterCall));
}

TEST_CASE(TemplateParser_parse_pipe_with_arguments) {
    prebyte::TemplateLexer lexer("{{ name | replace(\"a\", \"b\") }}", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    const auto& interpolation = static_cast<const prebyte::InterpolationNode&>(*document->children[0]);
    REQUIRE_EQ(static_cast<int>(interpolation.expression->kind), static_cast<int>(prebyte::ExpressionKind::FilterCall));

    const auto& filter = static_cast<const prebyte::FilterCallExpr&>(*interpolation.expression);
    REQUIRE_EQ(filter.name, std::string("replace"));
    REQUIRE_EQ(filter.arguments.size(), static_cast<std::size_t>(2));
}

TEST_CASE(TemplateParser_pipe_has_lower_precedence_than_comparison) {
    prebyte::TemplateLexer lexer("{{ user.name == \"Ada\" | upper }}", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    const auto& interpolation = static_cast<const prebyte::InterpolationNode&>(*document->children[0]);
    REQUIRE_EQ(static_cast<int>(interpolation.expression->kind), static_cast<int>(prebyte::ExpressionKind::FilterCall));

    const auto& filter = static_cast<const prebyte::FilterCallExpr&>(*interpolation.expression);
    REQUIRE(filter.input != nullptr);
    REQUIRE_EQ(static_cast<int>(filter.input->kind), static_cast<int>(prebyte::ExpressionKind::Binary));
}

TEST_CASE(TemplateParser_parse_comparison_and_in_operators) {
    prebyte::TemplateLexer lexer("{{ if price >= min_price && sku in allowed }}ok{{ endif }}", "inline");
    prebyte::TemplateParser parser(lexer.lex(), prebyte::TemplateParserOptions{.enable_loops = true});
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    const auto& if_node = static_cast<const prebyte::IfNode&>(*document->children[0]);
    REQUIRE(if_node.branches[0].condition != nullptr);
    REQUIRE_EQ(static_cast<int>(if_node.branches[0].condition->kind), static_cast<int>(prebyte::ExpressionKind::Binary));
}

TEST_CASE(TemplateParser_reject_reserved_set_target) {
    prebyte::TemplateLexer lexer("{{ set loop = item }}", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    REQUIRE_THROWS_AS(parser.parse_document(), prebyte::DiagnosticError);
}

TEST_CASE(TemplateParser_capture_trim_markers_on_interpolation) {
    prebyte::TemplateLexer lexer("A {{- name -}} B", "inline");
    prebyte::TemplateParser parser(lexer.lex());
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    const auto& interpolation = static_cast<const prebyte::InterpolationNode&>(*document->children[1]);
    REQUIRE(interpolation.trim_left);
    REQUIRE(interpolation.trim_right);
}

TEST_CASE(TemplateParser_parse_function_definition_and_call) {
    prebyte::TemplateLexer lexer("{{ fn greet(name) }}Hello {{ name }}{{ endfn }}{{ greet(\"Ada\") }}", "inline");
    prebyte::TemplateParser parser(lexer.lex(), prebyte::TemplateParserOptions{.enable_loops = true});
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(static_cast<int>(document->children[0]->kind), static_cast<int>(prebyte::TemplateNodeKind::FunctionDef));

    const auto& function = static_cast<const prebyte::FunctionDefNode&>(*document->children[0]);
    REQUIRE_EQ(function.name, std::string("greet"));
    REQUIRE_EQ(function.parameters.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(function.parameters[0], std::string("name"));
    REQUIRE_EQ(static_cast<int>(function.mode), static_cast<int>(prebyte::FunctionMode::Template));

    const auto& interpolation = static_cast<const prebyte::InterpolationNode&>(*document->children[1]);
    REQUIRE_EQ(static_cast<int>(interpolation.expression->kind), static_cast<int>(prebyte::ExpressionKind::FunctionCall));
}

TEST_CASE(TemplateParser_parse_lua_function_definition) {
    prebyte::TemplateLexer lexer("{{ fn pick() lua:block }}return { name = \"Ada\" }{{ endfn }}", "inline");
    prebyte::TemplateParser parser(lexer.lex(), prebyte::TemplateParserOptions{.enable_loops = true});
    std::unique_ptr<prebyte::DocumentNode> document = parser.parse_document();

    REQUIRE_EQ(document->children.size(), static_cast<std::size_t>(1));
    const auto& function = static_cast<const prebyte::FunctionDefNode&>(*document->children[0]);
    REQUIRE_EQ(static_cast<int>(function.mode), static_cast<int>(prebyte::FunctionMode::Lua));
    REQUIRE_EQ(function.lua_source, std::string("return { name = \"Ada\" }"));
}

TEST_CASE(TemplateParser_reject_duplicate_function_parameter_name) {
    prebyte::TemplateLexer lexer("{{ fn bad(name, name) }}x{{ endfn }}", "inline");
    prebyte::TemplateParser parser(lexer.lex(), prebyte::TemplateParserOptions{.enable_loops = true});
    REQUIRE_THROWS_AS(parser.parse_document(), prebyte::DiagnosticError);
}
