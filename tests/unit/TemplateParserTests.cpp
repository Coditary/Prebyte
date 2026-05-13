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
    REQUIRE_EQ(static_cast<int>(interpolation.expression->kind), static_cast<int>(prebyte::ExpressionKind::Identifier));

    const auto& identifier = static_cast<const prebyte::IdentifierExpr&>(*interpolation.expression);
    REQUIRE_EQ(identifier.name, std::string("ARGS[0]"));
}
