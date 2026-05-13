#include "template/parser/TemplateParser.h"

#include "support/Diagnostic.h"

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
    if (!is_at_end()) {
        ++current_;
    }
    return tokens_[current_ - 1];
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

std::vector<TemplateNodePtr> TemplateParser::parse_nodes_until(const std::vector<TemplateTokenType>& terminators) {
    std::vector<TemplateNodePtr> nodes;
    while (!is_at_end()) {
        if (is_terminator_ahead(terminators)) {
            break;
        }
        nodes.push_back(parse_node());
    }
    return nodes;
}

TemplateNodePtr TemplateParser::parse_node() {
    if (check(TemplateTokenType::Text)) {
        const TemplateToken token = advance();
        return std::make_unique<TextNode>(token.lexeme, token.span);
    }
    if (check(TemplateTokenType::TagOpen)) {
        return parse_tag();
    }

    throw DiagnosticError(make_error(peek(), "Expected text or tag"));
}

TemplateNodePtr TemplateParser::parse_tag() {
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordIf, 1)) {
        return parse_if_block();
    }
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordLua, 1)) {
        return parse_lua_expr();
    }
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordLuaBlock, 1)) {
        return parse_lua_block();
    }
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::Identifier, 1)) {
        const std::string directive = peek(1).lexeme;
        if ((directive == "for" || directive == "while") && !options_.enable_loops) {
            throw DiagnosticError(make_error(peek(1), "Loop directives are reserved for a later phase"));
        }
    }
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordInclude, 1)) {
        return parse_include();
    }
    if (check(TemplateTokenType::TagOpen) && (check(TemplateTokenType::KeywordElseIf, 1)
        || check(TemplateTokenType::KeywordElse, 1) || check(TemplateTokenType::KeywordEndIf, 1))) {
        throw DiagnosticError(make_error(peek(1), "Unexpected control-flow terminator"));
    }
    return parse_interpolation();
}

std::unique_ptr<LuaExprNode> TemplateParser::parse_lua_expr() {
    const TemplateToken start = consume(TemplateTokenType::TagOpen, "Expected tag start");
    consume(TemplateTokenType::KeywordLua, "Expected lua keyword");
    const TemplateToken source = consume(TemplateTokenType::String, "Expected Lua string literal");
    const TemplateToken end = consume(TemplateTokenType::TagClose, "Expected tag end after lua expression");

    SourceSpan span = start.span;
    span.end = end.span.end;
    return std::make_unique<LuaExprNode>(source.lexeme, span);
}

std::unique_ptr<ExpressionNode> TemplateParser::parse_if_condition(const std::string& branch_name) {
    if (match(TemplateTokenType::KeywordLuaBlock)) {
        const TemplateToken start = tokens_[current_ - 1];
        consume(TemplateTokenType::TagClose, "Expected tag end after " + branch_name + " lua:block");
        const std::string source = parse_raw_lua_body();

        consume(TemplateTokenType::TagOpen, "Expected tag start before endlua");
        consume(TemplateTokenType::KeywordEndLua, "Expected endlua keyword");
        const TemplateToken end = consume(TemplateTokenType::TagClose, "Expected tag end after endlua");

        SourceSpan span = start.span;
        span.end = end.span.end;
        return std::make_unique<LuaCallExpr>(source, span);
    }

    auto condition = parse_expression();
    consume(TemplateTokenType::TagClose, "Expected tag end after " + branch_name + " expression");
    return condition;
}

std::unique_ptr<LuaBlockNode> TemplateParser::parse_lua_block() {
    const TemplateToken start = consume(TemplateTokenType::TagOpen, "Expected tag start");
    consume(TemplateTokenType::KeywordLuaBlock, "Expected lua:block keyword");
    consume(TemplateTokenType::TagClose, "Expected tag end after lua:block");

    const std::string source = parse_raw_lua_body();

    consume(TemplateTokenType::TagOpen, "Expected tag start before endlua");
    consume(TemplateTokenType::KeywordEndLua, "Expected endlua keyword");
    const TemplateToken end = consume(TemplateTokenType::TagClose, "Expected tag end after endlua");

    SourceSpan span = start.span;
    span.end = end.span.end;
    return std::make_unique<LuaBlockNode>(source, span);
}

std::unique_ptr<IncludeNode> TemplateParser::parse_include() {
    const TemplateToken start = consume(TemplateTokenType::TagOpen, "Expected tag start");
    consume(TemplateTokenType::KeywordInclude, "Expected include keyword");
    const TemplateToken path = consume(TemplateTokenType::String, "Expected include path string");
    const TemplateToken end = consume(TemplateTokenType::TagClose, "Expected tag end after include");

    SourceSpan span = start.span;
    span.end = end.span.end;
    return std::make_unique<IncludeNode>(path.lexeme, span);
}

std::unique_ptr<IfNode> TemplateParser::parse_if_block() {
    const TemplateToken start = consume(TemplateTokenType::TagOpen, "Expected tag start");
    consume(TemplateTokenType::KeywordIf, "Expected if keyword");
    auto node = std::make_unique<IfNode>(start.span);

    IfBranch if_branch;
    if_branch.condition = parse_if_condition("if");
    if_branch.body = parse_nodes_until({TemplateTokenType::KeywordElseIf, TemplateTokenType::KeywordElse, TemplateTokenType::KeywordEndIf});
    node->branches.push_back(std::move(if_branch));

    while (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordElseIf, 1)) {
        advance();
        advance();

        IfBranch branch;
        branch.condition = parse_if_condition("elseif");
        branch.body = parse_nodes_until({TemplateTokenType::KeywordElseIf, TemplateTokenType::KeywordElse, TemplateTokenType::KeywordEndIf});
        node->branches.push_back(std::move(branch));
    }

    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordElse, 1)) {
        advance();
        advance();
        consume(TemplateTokenType::TagClose, "Expected tag end after else");

        IfBranch else_branch;
        else_branch.body = parse_nodes_until({TemplateTokenType::KeywordEndIf});
        node->branches.push_back(std::move(else_branch));
    }

    consume(TemplateTokenType::TagOpen, "Expected tag start before endif");
    consume(TemplateTokenType::KeywordEndIf, "Expected endif keyword");
    const TemplateToken end = consume(TemplateTokenType::TagClose, "Expected tag end after endif");
    node->span.end = end.span.end;

    return node;
}

std::unique_ptr<InterpolationNode> TemplateParser::parse_interpolation() {
    const TemplateToken start = consume(TemplateTokenType::TagOpen, "Expected tag start");
    auto expression = parse_expression();
    const TemplateToken end = consume(TemplateTokenType::TagClose, "Expected tag end after expression");

    SourceSpan span = start.span;
    span.end = end.span.end;
    return std::make_unique<InterpolationNode>(std::move(expression), span);
}

std::unique_ptr<ExpressionNode> TemplateParser::parse_expression() {
    return parse_or();
}

std::unique_ptr<ExpressionNode> TemplateParser::parse_or() {
    auto expression = parse_and();
    while (match(TemplateTokenType::OrOr)) {
        const TemplateToken op = tokens_[current_ - 1];
        auto right = parse_and();
        SourceSpan span = expression->span;
        span.end = right->span.end;
        expression = std::make_unique<BinaryExpr>(std::move(expression), op.lexeme, std::move(right), span);
    }
    return expression;
}

std::unique_ptr<ExpressionNode> TemplateParser::parse_and() {
    auto expression = parse_equality();
    while (match(TemplateTokenType::AndAnd)) {
        const TemplateToken op = tokens_[current_ - 1];
        auto right = parse_equality();
        SourceSpan span = expression->span;
        span.end = right->span.end;
        expression = std::make_unique<BinaryExpr>(std::move(expression), op.lexeme, std::move(right), span);
    }
    return expression;
}

std::unique_ptr<ExpressionNode> TemplateParser::parse_equality() {
    auto expression = parse_unary();
    while (match(TemplateTokenType::EqualEqual) || match(TemplateTokenType::BangEqual)) {
        const TemplateToken op = tokens_[current_ - 1];
        auto right = parse_unary();
        SourceSpan span = expression->span;
        span.end = right->span.end;
        expression = std::make_unique<BinaryExpr>(std::move(expression), op.lexeme, std::move(right), span);
    }
    return expression;
}

std::unique_ptr<ExpressionNode> TemplateParser::parse_unary() {
    if (match(TemplateTokenType::Bang)) {
        const TemplateToken op = tokens_[current_ - 1];
        auto operand = parse_unary();
        SourceSpan span = op.span;
        span.end = operand->span.end;
        return std::make_unique<UnaryExpr>(op.lexeme, std::move(operand), span);
    }
    return parse_primary();
}

std::unique_ptr<ExpressionNode> TemplateParser::parse_primary() {
    if (match(TemplateTokenType::KeywordLua)) {
        const TemplateToken start = tokens_[current_ - 1];
        consume(TemplateTokenType::LeftParen, "Expected '(' after lua");
        const TemplateToken source = consume(TemplateTokenType::String, "Expected Lua string literal");
        const TemplateToken end = consume(TemplateTokenType::RightParen, "Expected ')' after Lua string");
        SourceSpan span = start.span;
        span.end = end.span.end;
        return std::make_unique<LuaCallExpr>(source.lexeme, span);
    }
    if (match(TemplateTokenType::Identifier)) {
        const TemplateToken token = tokens_[current_ - 1];
        return std::make_unique<IdentifierExpr>(token.lexeme, token.span);
    }
    if (match(TemplateTokenType::String)) {
        const TemplateToken token = tokens_[current_ - 1];
        return std::make_unique<StringExpr>(token.lexeme, token.span);
    }
    if (match(TemplateTokenType::Number)) {
        const TemplateToken token = tokens_[current_ - 1];
        return std::make_unique<NumberExpr>(std::stod(token.lexeme), token.lexeme, token.span);
    }
    if (match(TemplateTokenType::Boolean)) {
        const TemplateToken token = tokens_[current_ - 1];
        return std::make_unique<BoolExpr>(token.lexeme == "true", token.span);
    }
    if (match(TemplateTokenType::LeftParen)) {
        const TemplateToken start = tokens_[current_ - 1];
        auto expression = parse_expression();
        const TemplateToken end = consume(TemplateTokenType::RightParen, "Expected ')' after expression");
        SourceSpan span = start.span;
        span.end = end.span.end;
        return std::make_unique<GroupedExpr>(std::move(expression), span);
    }

    throw DiagnosticError(make_error(peek(), "Expected expression"));
}

std::string TemplateParser::parse_raw_lua_body() {
    std::string source;
    while (!is_at_end()) {
        if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordEndLua, 1)) {
            break;
        }
        source += advance().lexeme;
    }

    if (source.empty()) {
        throw DiagnosticError(make_error(peek(), "Expected raw Lua block body"));
    }

    return source;
}

Diagnostic TemplateParser::make_error(const TemplateToken& token, const std::string& message) const {
    Diagnostic diagnostic;
    diagnostic.code = "PARSE001";
    diagnostic.message = message;
    diagnostic.span = token.span;
    diagnostic.snippet = token.lexeme;
    return diagnostic;
}

TemplateNodePtr TemplateParser::parse_loop_placeholder() {
    throw DiagnosticError(make_error(peek(), "Loop directives are not implemented yet"));
}

}
