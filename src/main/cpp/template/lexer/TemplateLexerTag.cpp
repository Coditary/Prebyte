#include "template/lexer/TemplateLexer.h"

#include <cctype>

#include "support/Diagnostic.h"
#include "support/TextUtil.h"
#include "template/lexer/TemplateLexerInternals.h"

namespace prebyte {

namespace {

struct PunctuationRule {
    std::string_view lexeme;
    TemplateTokenType type;
};

constexpr PunctuationRule kTwoCharPunctuation[] = {
    {"&&", TemplateTokenType::AndAnd},
    {"||", TemplateTokenType::OrOr},
    {"==", TemplateTokenType::EqualEqual},
    {"<=", TemplateTokenType::LessEqual},
    {">=", TemplateTokenType::GreaterEqual},
    {"!=", TemplateTokenType::BangEqual},
};

constexpr PunctuationRule kOneCharPunctuation[] = {
    {"!", TemplateTokenType::Bang},
    {"(", TemplateTokenType::LeftParen},
    {"|", TemplateTokenType::Pipe},
    {"[", TemplateTokenType::LeftBracket},
    {")", TemplateTokenType::RightParen},
    {"]", TemplateTokenType::RightBracket},
    {".", TemplateTokenType::Dot},
    {",", TemplateTokenType::Comma},
    {"=", TemplateTokenType::Equal},
    {"<", TemplateTokenType::Less},
    {">", TemplateTokenType::Greater},
};

} // namespace

bool TemplateLexer::try_lex_tag_close() {
    if (match_literal(tag_suffix_)) {
        const SourceLocation start = current_location();
        advance_literal(tag_suffix_);
        add_token(TemplateTokenType::TagClose, std::string(tag_suffix_), start, false, false);
        inside_tag_ = false;
        return true;
    }

    if (peek() == '-' && match_literal(std::string("-") + std::string(tag_suffix_))) {
        const SourceLocation start = current_location();
        advance();
        advance_literal(tag_suffix_);
        add_token(TemplateTokenType::TagClose, std::string(tag_suffix_), start, false, true);
        trim_next_text_left_ = true;
        inside_tag_ = false;
        return true;
    }

    return false;
}

bool TemplateLexer::try_lex_tag_punctuation() {
    const SourceLocation start = current_location();

    for (const PunctuationRule& rule : kTwoCharPunctuation) {
        if (match_literal(rule.lexeme)) {
            advance_literal(rule.lexeme);
            add_token(rule.type, std::string(rule.lexeme), start);
            return true;
        }
    }

    for (const PunctuationRule& rule : kOneCharPunctuation) {
        if (peek() == rule.lexeme.front()) {
            advance();
            add_token(rule.type, std::string(rule.lexeme), start);
            return true;
        }
    }

    return false;
}

void TemplateLexer::skip_tag_whitespace() {
    while (!is_at_end() && std::isspace(static_cast<unsigned char>(peek())) != 0) {
        advance();
    }
}

void TemplateLexer::lex_inside_tag() {
    skip_tag_whitespace();
    if (is_at_end()) {
        return;
    }

    if (try_lex_tag_close()) {
        return;
    }

    const char ch = peek();
    if (text::is_identifier_start(ch)) {
        lex_identifier_or_keyword();
        return;
    }
    if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
        lex_number();
        return;
    }
    if (ch == '"') {
        lex_string();
        return;
    }
    if (try_lex_tag_punctuation()) {
        return;
    }

    const SourceLocation start = current_location();
    throw DiagnosticError(make_lexer_error(std::string("Unexpected character in tag: ") + ch, file_path_, start));
}

}
