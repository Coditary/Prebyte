#include "template/lexer/TemplateLexerInternals.h"

#include "support/Diagnostic.h"

namespace prebyte {

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
