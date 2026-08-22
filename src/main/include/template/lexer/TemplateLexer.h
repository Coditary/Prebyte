#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "template/lexer/TemplateToken.h"

namespace prebyte {

// Scans template source into tokens. Operates in two modes:
//   1. Text mode  – copies literal output until an opening tag delimiter is found.
//   2. Tag mode   – tokenizes tag contents (keywords, literals, punctuation) until closing delimiter.
class TemplateLexer {
public:
    TemplateLexer(std::string_view source, std::string file_path, std::string_view tag_prefix = "{{",
                  std::string_view tag_suffix = "}}");

    std::vector<TemplateToken> lex();

private:
    // Cursor / location helpers
    char peek(std::size_t offset = 0) const;
    bool is_at_end() const;
    bool match_literal(std::string_view literal) const;
    char advance();
    void advance_literal(std::string_view literal);
    SourceLocation current_location() const;
    SourceSpan make_span(SourceLocation start) const;

    // Token emission
    void add_token(TemplateTokenType type, std::string lexeme, SourceLocation start);
    void add_token(TemplateTokenType type, std::string lexeme, SourceLocation start, bool trim_left, bool trim_right);

    // Mode-specific scanners (implemented in separate translation units)
    void lex_text();
    void lex_inside_tag();
    void skip_tag_whitespace();
    bool try_lex_tag_close();
    bool try_lex_tag_punctuation();
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
    bool trim_next_text_left_ = false;
};

}
