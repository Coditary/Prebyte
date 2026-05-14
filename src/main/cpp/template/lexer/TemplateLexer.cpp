#include "template/lexer/TemplateLexer.h"

#include "support/Diagnostic.h"
#include "support/TextUtil.h"

namespace prebyte {

namespace {

Diagnostic make_lexer_error(const std::string& message, const std::string& file_path, SourceLocation location) {
    Diagnostic diagnostic;
    diagnostic.code = "LEX001";
    diagnostic.message = message;
    diagnostic.span.file_path = file_path;
    diagnostic.span.start = location;
    diagnostic.span.end = location;
    return diagnostic;
}

void trim_right_ascii_whitespace(std::string& text) {
    while (!text.empty()) {
        const char ch = text.back();
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            break;
        }
        text.pop_back();
    }
}

void trim_left_ascii_whitespace(std::string& text) {
    std::size_t start = 0;
    while (start < text.size()) {
        const char ch = text[start];
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            break;
        }
        ++start;
    }
    if (start != 0) {
        text.erase(0, start);
    }
}

}

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

void TemplateLexer::add_token(TemplateTokenType type, std::string lexeme, SourceLocation start,
                              bool trim_left, bool trim_right) {
    tokens_.push_back(TemplateToken{type, std::move(lexeme), make_span(start), trim_left, trim_right});
}

void TemplateLexer::lex_text() {
    const SourceLocation start = current_location();
    std::string text;
    while (!is_at_end() && !match_literal(tag_prefix_)) {
        text.push_back(advance());
    }

    if (trim_next_text_left_) {
        trim_left_ascii_whitespace(text);
        trim_next_text_left_ = false;
    }

    if (!text.empty()) {
        add_token(TemplateTokenType::Text, text, start);
    }

    if (!is_at_end() && match_literal(tag_prefix_)) {
        const SourceLocation tag_start = current_location();
        advance_literal(tag_prefix_);
        bool trim_left = false;
        if (!is_at_end() && peek() == '-') {
            trim_left = true;
            advance();
        }
        if (trim_left && !tokens_.empty() && tokens_.back().type == TemplateTokenType::Text) {
            trim_right_ascii_whitespace(tokens_.back().lexeme);
            if (tokens_.back().lexeme.empty()) {
                tokens_.pop_back();
            }
        }
        add_token(TemplateTokenType::TagOpen, std::string(tag_prefix_), tag_start, trim_left, false);
        inside_tag_ = true;
    }
}

void TemplateLexer::lex_inside_tag() {
    skip_tag_whitespace();
    if (is_at_end()) {
        return;
    }

    if (match_literal(tag_suffix_)) {
        const SourceLocation start = current_location();
        advance_literal(tag_suffix_);
        add_token(TemplateTokenType::TagClose, std::string(tag_suffix_), start, false, false);
        inside_tag_ = false;
        return;
    }

    if (peek() == '-' && match_literal(std::string("-") + std::string(tag_suffix_))) {
        const SourceLocation start = current_location();
        advance();
        advance_literal(tag_suffix_);
        add_token(TemplateTokenType::TagClose, std::string(tag_suffix_), start, false, true);
        trim_next_text_left_ = true;
        inside_tag_ = false;
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

    const SourceLocation start = current_location();
    if (match_literal("&&")) {
        advance_literal("&&");
        add_token(TemplateTokenType::AndAnd, "&&", start);
        return;
    }
    if (match_literal("||")) {
        advance_literal("||");
        add_token(TemplateTokenType::OrOr, "||", start);
        return;
    }
    if (match_literal("==")) {
        advance_literal("==");
        add_token(TemplateTokenType::EqualEqual, "==", start);
        return;
    }
    if (match_literal("<=")) {
        advance_literal("<=");
        add_token(TemplateTokenType::LessEqual, "<=", start);
        return;
    }
    if (match_literal(">=")) {
        advance_literal(">=");
        add_token(TemplateTokenType::GreaterEqual, ">=", start);
        return;
    }
    if (match_literal("!=")) {
        advance_literal("!=");
        add_token(TemplateTokenType::BangEqual, "!=", start);
        return;
    }
    if (ch == '!') {
        advance();
        add_token(TemplateTokenType::Bang, "!", start);
        return;
    }
    if (ch == '(') {
        advance();
        add_token(TemplateTokenType::LeftParen, "(", start);
        return;
    }
    if (ch == '|') {
        advance();
        add_token(TemplateTokenType::Pipe, "|", start);
        return;
    }
    if (ch == '[') {
        advance();
        add_token(TemplateTokenType::LeftBracket, "[", start);
        return;
    }
    if (ch == ')') {
        advance();
        add_token(TemplateTokenType::RightParen, ")", start);
        return;
    }
    if (ch == ']') {
        advance();
        add_token(TemplateTokenType::RightBracket, "]", start);
        return;
    }
    if (ch == '.') {
        advance();
        add_token(TemplateTokenType::Dot, ".", start);
        return;
    }
    if (ch == ',') {
        advance();
        add_token(TemplateTokenType::Comma, ",", start);
        return;
    }
    if (ch == '=') {
        advance();
        add_token(TemplateTokenType::Equal, "=", start);
        return;
    }
    if (ch == '<') {
        advance();
        add_token(TemplateTokenType::Less, "<", start);
        return;
    }
    if (ch == '>') {
        advance();
        add_token(TemplateTokenType::Greater, ">", start);
        return;
    }

    throw DiagnosticError(make_lexer_error(std::string("Unexpected character in tag: ") + ch, file_path_, start));
}

void TemplateLexer::skip_tag_whitespace() {
    while (!is_at_end() && std::isspace(static_cast<unsigned char>(peek())) != 0) {
        advance();
    }
}

void TemplateLexer::lex_identifier_or_keyword() {
    const SourceLocation start = current_location();
    std::string value;
    while (!is_at_end() && text::is_identifier_part(peek())) {
        value.push_back(advance());
    }

    if (value == "if") {
        add_token(TemplateTokenType::KeywordIf, value, start);
        return;
    }
    if (value == "for") {
        add_token(TemplateTokenType::KeywordFor, value, start);
        return;
    }
    if (value == "in") {
        add_token(TemplateTokenType::KeywordIn, value, start);
        return;
    }
    if (value == "elseif") {
        add_token(TemplateTokenType::KeywordElseIf, value, start);
        return;
    }
    if (value == "else") {
        add_token(TemplateTokenType::KeywordElse, value, start);
        return;
    }
    if (value == "endif") {
        add_token(TemplateTokenType::KeywordEndIf, value, start);
        return;
    }
    if (value == "endfor") {
        add_token(TemplateTokenType::KeywordEndFor, value, start);
        return;
    }
    if (value == "include") {
        add_token(TemplateTokenType::KeywordInclude, value, start);
        return;
    }
    if (value == "set") {
        add_token(TemplateTokenType::KeywordSet, value, start);
        return;
    }
    if (value == "fn") {
        add_token(TemplateTokenType::KeywordFn, value, start);
        return;
    }
    if (value == "endfn") {
        add_token(TemplateTokenType::KeywordEndFn, value, start);
        return;
    }
    if (value == "lua") {
        add_token(TemplateTokenType::KeywordLua, value, start);
        return;
    }
    if (value == "lua:block") {
        add_token(TemplateTokenType::KeywordLuaBlock, value, start);
        return;
    }
    if (value == "endlua") {
        add_token(TemplateTokenType::KeywordEndLua, value, start);
        return;
    }
    if (value == "true" || value == "false") {
        add_token(TemplateTokenType::Boolean, value, start);
        return;
    }
    if (value == "in") {
        add_token(TemplateTokenType::KeywordIn, value, start);
        return;
    }

    add_token(TemplateTokenType::Identifier, value, start);
}

void TemplateLexer::lex_string() {
    const SourceLocation start = current_location();
    advance();
    std::string value;

    while (!is_at_end() && peek() != '"') {
        char ch = advance();
        if (ch == '\\') {
            const char escaped = advance();
            switch (escaped) {
            case 'n': value.push_back('\n'); break;
            case 't': value.push_back('\t'); break;
            case '\\': value.push_back('\\'); break;
            case '"': value.push_back('"'); break;
            default:
                throw DiagnosticError(make_lexer_error("Unsupported escape sequence", file_path_, current_location()));
            }
            continue;
        }
        value.push_back(ch);
    }

    if (is_at_end()) {
        throw DiagnosticError(make_lexer_error("Unterminated string literal", file_path_, start));
    }

    advance();
    add_token(TemplateTokenType::String, value, start);
}

void TemplateLexer::lex_number() {
    const SourceLocation start = current_location();
    std::string value;
    while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
        value.push_back(advance());
    }
    if (!is_at_end() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1))) != 0) {
        value.push_back(advance());
        while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            value.push_back(advance());
        }
    }
    add_token(TemplateTokenType::Number, value, start);
}

}
