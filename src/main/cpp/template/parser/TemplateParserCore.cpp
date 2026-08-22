#include "template/parser/TemplateParser.h"

#include "template/parser/TemplateParserInternals.h"

namespace prebyte {

TemplateParser::TemplateParser(std::vector<TemplateToken> tokens, TemplateParserOptions options)
    : tokens_(std::move(tokens)), options_(options) {}

std::unique_ptr<DocumentNode> TemplateParser::parse_document() {
    auto document = std::make_unique<DocumentNode>();
    document->children = parse_nodes_until({});
    consume(TemplateTokenType::EndOfFile, "Expected end of file");
    return document;
}

const TemplateToken& TemplateParser::peek(std::size_t offset) const {
    if (tokens_.empty()) {
        throw DiagnosticError(make_error(TemplateToken{TemplateTokenType::EndOfFile, "", {}}, "Empty template token stream"));
    }

    const std::size_t index = current_ + offset;
    if (index >= tokens_.size()) {
        return tokens_.back();
    }
    return tokens_[index];
}

bool TemplateParser::is_at_end() const {
    return peek().type == TemplateTokenType::EndOfFile;
}

bool TemplateParser::check(TemplateTokenType type, std::size_t offset) const {
    return peek(offset).type == type;
}

bool TemplateParser::match(TemplateTokenType type) {
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

const TemplateToken& TemplateParser::advance() {
    if (tokens_.empty()) {
        throw DiagnosticError(make_error(TemplateToken{TemplateTokenType::EndOfFile, "", {}}, "Empty template token stream"));
    }
    if (current_ >= tokens_.size()) {
        return tokens_.back();
    }

    return tokens_[current_++];
}

const TemplateToken& TemplateParser::consume(TemplateTokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    throw DiagnosticError(make_error(peek(), message));
}

bool TemplateParser::is_terminator_ahead(const std::vector<TemplateTokenType>& terminators) const {
    if (terminators.empty()) {
        return false;
    }
    if (!check(TemplateTokenType::TagOpen) || is_at_end()) {
        return false;
    }
    for (TemplateTokenType type : terminators) {
        if (check(type, 1)) {
            return true;
        }
    }
    return false;
}

Diagnostic TemplateParser::make_error(const TemplateToken& token, const std::string& message) const {
    Diagnostic diagnostic;
    diagnostic.code = "PARSE001";
    diagnostic.message = message;
    diagnostic.span = token.span;
    diagnostic.snippet = token.lexeme;
    return diagnostic;
}

void TemplateParser::push_expression_depth() {
    if (expression_depth_ >= kMaxParserExpressionDepth) {
        throw DiagnosticError(make_error(peek(), "Expression nesting is too deep"));
    }
    ++expression_depth_;
}

void TemplateParser::pop_expression_depth() {
    if (expression_depth_ > 0) {
        --expression_depth_;
    }
}

}
