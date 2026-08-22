#include "template/lexer/TemplateLexer.h"

#include <cctype>

#include "support/Diagnostic.h"
#include "support/TextUtil.h"
#include "template/lexer/TemplateLexerInternals.h"
#include "template/lexer/TemplateLexerKeywords.h"

namespace prebyte {

void TemplateLexer::lex_identifier_or_keyword() {
    const SourceLocation start = current_location();
    std::string value;
    while (!is_at_end() && text::is_identifier_part(peek())) {
        value.push_back(advance());
    }

    if (const std::optional<TemplateTokenType> keyword = template_lexer::lookup_keyword(value)) {
        add_token(*keyword, value, start);
        return;
    }

    add_token(TemplateTokenType::Identifier, value, start);
}

void TemplateLexer::lex_string() {
    const SourceLocation start = current_location();
    advance();
    std::string value;

    while (!is_at_end() && peek() != '"') {
        const char ch = advance();
        if (ch == '\\') {
            if (is_at_end()) {
                throw DiagnosticError(make_lexer_error("Unterminated string literal", file_path_, start));
            }
            const char escaped = advance();
            switch (escaped) {
            case 'n':
                value.push_back('\n');
                break;
            case 't':
                value.push_back('\t');
                break;
            case '\\':
                value.push_back('\\');
                break;
            case '"':
                value.push_back('"');
                break;
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
