#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "template/lexer/TemplateToken.h"

namespace prebyte {

class TemplateLexer {
public:
    TemplateLexer(std::string_view source, std::string file_path, std::string_view tag_prefix = "{{",
                  std::string_view tag_suffix = "}}");

    std::vector<TemplateToken> lex();

private:
    char peek(std::size_t offset = 0) const;
    bool is_at_end() const;
    bool match_literal(std::string_view literal) const;
    char advance();
    void advance_literal(std::string_view literal);
    SourceLocation current_location() const;
    SourceSpan make_span(SourceLocation start) const;
    void add_token(TemplateTokenType type, std::string lexeme, SourceLocation start);
    void lex_text();
    void lex_inside_tag();
    void skip_tag_whitespace();
    void lex_identifier_or_keyword();
    void lex_string();
    void lex_number();

    std::string_view source_;
    std::string file_path_;
    std::string_view tag_prefix_;
    std::string_view tag_suffix_;
    std::vector<TemplateToken> tokens_;
    std::size_t index_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
    bool inside_tag_ = false;
};

}
