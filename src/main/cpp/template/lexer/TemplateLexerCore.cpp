#include "template/lexer/TemplateLexer.h"

#include "support/Diagnostic.h"
#include "template/lexer/TemplateLexerInternals.h"

namespace prebyte {

TemplateLexer::TemplateLexer(std::string_view source, std::string file_path, std::string_view tag_prefix,
                             std::string_view tag_suffix)
    : source_(source), file_path_(std::move(file_path)), tag_prefix_(tag_prefix), tag_suffix_(tag_suffix) {}

std::vector<TemplateToken> TemplateLexer::lex() {
    while (!is_at_end()) {
        if (!inside_tag_) {
            lex_text();
        } else {
            lex_inside_tag();
        }
    }

    if (inside_tag_) {
        throw DiagnosticError(make_lexer_error("Unclosed tag", file_path_, current_location()));
    }

    add_token(TemplateTokenType::EndOfFile, "", current_location());
    return tokens_;
}

char TemplateLexer::peek(std::size_t offset) const {
    const std::size_t target = index_ + offset;
    if (target >= source_.size()) {
        return '\0';
    }
    return source_[target];
}

bool TemplateLexer::is_at_end() const {
    return index_ >= source_.size();
}

bool TemplateLexer::match_literal(std::string_view literal) const {
    return source_.compare(index_, literal.size(), literal) == 0;
}

char TemplateLexer::advance() {
    if (is_at_end()) {
        return '\0';
    }
    const char ch = source_[index_++];
    if (ch == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return ch;
}

void TemplateLexer::advance_literal(std::string_view literal) {
    for (std::size_t i = 0; i < literal.size(); ++i) {
        advance();
    }
}

SourceLocation TemplateLexer::current_location() const {
    return SourceLocation{index_, line_, column_};
}

SourceSpan TemplateLexer::make_span(SourceLocation start) const {
    SourceSpan span;
    span.file_path = file_path_;
    span.start = start;
    span.end = current_location();
    return span;
}

void TemplateLexer::add_token(TemplateTokenType type, std::string lexeme, SourceLocation start) {
    add_token(type, std::move(lexeme), start, false, false);
}

void TemplateLexer::add_token(TemplateTokenType type, std::string lexeme, SourceLocation start, bool trim_left,
                              bool trim_right) {
    tokens_.push_back(TemplateToken{type, std::move(lexeme), make_span(start), trim_left, trim_right});
}

}
