#include "template/lexer/TemplateLexer.h"

#include "template/lexer/TemplateLexerInternals.h"

namespace prebyte {

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

}
