#include "TestHarness.h"

#include "support/Diagnostic.h"
#include "template/lexer/TemplateLexer.h"

TEST_CASE(TemplateLexer_lex_function_keywords_and_call_tokens) {
    prebyte::TemplateLexer lexer("{{ fn greet(name) }}x{{ endfn }}{{ greet(\"Ada\") }}", "inline");
    const auto tokens = lexer.lex();

    REQUIRE_EQ(static_cast<int>(tokens[1].type), static_cast<int>(prebyte::TemplateTokenType::KeywordFn));
    REQUIRE_EQ(tokens[2].lexeme, std::string("greet"));
    REQUIRE_EQ(static_cast<int>(tokens[3].type), static_cast<int>(prebyte::TemplateTokenType::LeftParen));
    REQUIRE_EQ(tokens[4].lexeme, std::string("name"));
    REQUIRE_EQ(static_cast<int>(tokens[5].type), static_cast<int>(prebyte::TemplateTokenType::RightParen));
    REQUIRE_EQ(static_cast<int>(tokens[9].type), static_cast<int>(prebyte::TemplateTokenType::KeywordEndFn));
    REQUIRE_EQ(tokens[12].lexeme, std::string("greet"));
    REQUIRE_EQ(static_cast<int>(tokens[13].type), static_cast<int>(prebyte::TemplateTokenType::LeftParen));
    REQUIRE_EQ(tokens[14].lexeme, std::string("Ada"));
}

TEST_CASE(TemplateLexer_lex_lua_function_header) {
    prebyte::TemplateLexer lexer("{{ fn users() lua:block }}return {}{{ endfn }}", "inline");
    const auto tokens = lexer.lex();

    REQUIRE_EQ(static_cast<int>(tokens[1].type), static_cast<int>(prebyte::TemplateTokenType::KeywordFn));
    REQUIRE_EQ(tokens[2].lexeme, std::string("users"));
    REQUIRE_EQ(static_cast<int>(tokens[5].type), static_cast<int>(prebyte::TemplateTokenType::KeywordLuaBlock));
    REQUIRE_EQ(static_cast<int>(tokens[9].type), static_cast<int>(prebyte::TemplateTokenType::KeywordEndFn));
}

TEST_CASE(TemplateLexer_capture_trim_markers_around_function_call) {
    prebyte::TemplateLexer lexer("A {{- greet() -}} B", "inline");
    const auto tokens = lexer.lex();

    REQUIRE(tokens[1].trim_left);
    REQUIRE(tokens[5].trim_right);
}

TEST_CASE(TemplateLexer_support_custom_delimiters_with_function_syntax) {
    prebyte::TemplateLexer lexer("<< fn greet() >>x<< endfn >>", "inline", "<<", ">>");
    const auto tokens = lexer.lex();

    REQUIRE_EQ(static_cast<int>(tokens[0].type), static_cast<int>(prebyte::TemplateTokenType::TagOpen));
    REQUIRE_EQ(static_cast<int>(tokens[1].type), static_cast<int>(prebyte::TemplateTokenType::KeywordFn));
    REQUIRE_EQ(tokens[2].lexeme, std::string("greet"));
    REQUIRE_EQ(static_cast<int>(tokens[7].type), static_cast<int>(prebyte::TemplateTokenType::TagOpen));
    REQUIRE_EQ(static_cast<int>(tokens[8].type), static_cast<int>(prebyte::TemplateTokenType::KeywordEndFn));
}

TEST_CASE(TemplateLexer_fail_on_unclosed_tag_with_function_header) {
    prebyte::TemplateLexer lexer("{{ fn greet(name)", "inline");
    REQUIRE_THROWS_AS(lexer.lex(), prebyte::DiagnosticError);
}
