#include "template/parser/TemplateParser.h"

#include "support/Diagnostic.h"

#include <unordered_set>

namespace prebyte {

namespace {

constexpr std::string_view kBuiltinNames[] = {
    "__TIME__",
    "__LINE__",
    "__FILE__",
    "__FILENAME__",
    "__DIR__",
    "__EXTENSION__",
    "__DATE__",
    "__TIMESTAMP__",
    "__YEAR__",
    "__MONTH__",
    "__DAY__",
    "__UNIX_EPOCH__",
    "__USER__",
    "__HOST__",
    "__OS__",
    "__WORKING_DIR__",
    "__UUID__",
    "__RANDOM__",
};

bool is_builtin_name(std::string_view name) {
    for (const std::string_view builtin : kBuiltinNames) {
        if (builtin == name) {
            return true;
        }
    }
    return false;
}

bool is_keyword_name(std::string_view name) {
    return name == "if" || name == "elseif" || name == "else" || name == "endif"
        || name == "for" || name == "in" || name == "endfor" || name == "include"
        || name == "set" || name == "lua" || name == "fn" || name == "endfn"
        || name == "endlua" || name == "len";
}

bool is_reserved_name(std::string_view name) {
    return name == "loop" || name == "ARGS" || is_builtin_name(name) || is_keyword_name(name);
}

bool is_reserved_loop_binding(std::string_view name) {
    return is_reserved_name(name);
}

void apply_trim_flags(TemplateNode& node, const TemplateToken& start, const TemplateToken& end) {
    node.trim_left = start.trim_left;
    node.trim_right = end.trim_right;
}

}

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
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordFor, 1)) {
        if (!options_.enable_loops) {
            throw DiagnosticError(make_error(peek(1), "Loop directives are reserved for a later phase"));
        }
        return parse_for_block();
    }
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordSet, 1)) {
        return parse_set_statement();
    }
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordFn, 1)) {
        return parse_function_definition();
    }
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordLua, 1)) {
        return parse_lua_expr();
    }
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordLuaBlock, 1)) {
        return parse_lua_block();
    }
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::Identifier, 1)) {
        const std::string directive = peek(1).lexeme;
        if (directive == "while") {
            throw DiagnosticError(make_error(peek(1), "Loop directives are reserved for a later phase"));
        }
    }
    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordInclude, 1)) {
        return parse_include();
    }
    if (check(TemplateTokenType::TagOpen) && (check(TemplateTokenType::KeywordElseIf, 1)
        || check(TemplateTokenType::KeywordElse, 1) || check(TemplateTokenType::KeywordEndIf, 1)
        || check(TemplateTokenType::KeywordEndFor, 1) || check(TemplateTokenType::KeywordEndFn, 1))) {
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
        const std::string source = parse_raw_body_until(TemplateTokenType::KeywordEndLua);

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

    const std::string source = parse_raw_body_until(TemplateTokenType::KeywordEndLua);

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
    auto node = std::make_unique<IncludeNode>(path.lexeme, span);
    apply_trim_flags(*node, start, end);
    return node;
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
    apply_trim_flags(*node, start, end);

    return node;
}

std::unique_ptr<ForNode> TemplateParser::parse_for_block() {
    const TemplateToken start = consume(TemplateTokenType::TagOpen, "Expected tag start");
    consume(TemplateTokenType::KeywordFor, "Expected for keyword");

    const TemplateToken first = consume(TemplateTokenType::Identifier, "Expected loop binding name after for");
    if (is_reserved_loop_binding(first.lexeme)) {
        throw DiagnosticError(make_error(first, "Reserved loop variable name: " + first.lexeme));
    }

    std::optional<std::string> key_name;
    std::string value_name = first.lexeme;
    if (match(TemplateTokenType::Comma)) {
        key_name = first.lexeme;
        const TemplateToken second = consume(TemplateTokenType::Identifier, "Expected second loop binding name after ','");
        if (is_reserved_loop_binding(second.lexeme)) {
            throw DiagnosticError(make_error(second, "Reserved loop variable name: " + second.lexeme));
        }
        if (*key_name == second.lexeme) {
            throw DiagnosticError(make_error(second, "Loop binding names must be distinct"));
        }
        value_name = second.lexeme;
    }

    consume(TemplateTokenType::KeywordIn, "Expected 'in' after loop binding");
    auto iterable = parse_expression();
    consume(TemplateTokenType::TagClose, "Expected tag end after for expression");

    auto node = std::make_unique<ForNode>(value_name, std::move(iterable), key_name, start.span);
    node->body = parse_nodes_until({TemplateTokenType::KeywordElse, TemplateTokenType::KeywordEndFor});

    if (check(TemplateTokenType::TagOpen) && check(TemplateTokenType::KeywordElse, 1)) {
        advance();
        advance();
        consume(TemplateTokenType::TagClose, "Expected tag end after else");
        node->else_body = parse_nodes_until({TemplateTokenType::KeywordEndFor});
    }

    consume(TemplateTokenType::TagOpen, "Expected tag start before endfor");
    consume(TemplateTokenType::KeywordEndFor, "Expected endfor keyword");
    const TemplateToken end = consume(TemplateTokenType::TagClose, "Expected tag end after endfor");
    node->span.end = end.span.end;
    apply_trim_flags(*node, start, end);
    return node;
}

std::unique_ptr<SetNode> TemplateParser::parse_set_statement() {
    const TemplateToken start = consume(TemplateTokenType::TagOpen, "Expected tag start");
    consume(TemplateTokenType::KeywordSet, "Expected set keyword");
    const TemplateToken name = consume(TemplateTokenType::Identifier, "Expected variable name after set");
    if (is_reserved_loop_binding(name.lexeme)) {
        throw DiagnosticError(make_error(name, "Reserved variable name: " + name.lexeme));
    }
    consume(TemplateTokenType::Equal, "Expected '=' after set variable name");
    auto expression = parse_expression();
    const TemplateToken end = consume(TemplateTokenType::TagClose, "Expected tag end after set expression");

    SourceSpan span = start.span;
    span.end = end.span.end;
    auto node = std::make_unique<SetNode>(name.lexeme, std::move(expression), span);
    apply_trim_flags(*node, start, end);
    return node;
}

std::unique_ptr<FunctionDefNode> TemplateParser::parse_function_definition() {
    const TemplateToken start = consume(TemplateTokenType::TagOpen, "Expected tag start");
    consume(TemplateTokenType::KeywordFn, "Expected fn keyword");
    const TemplateToken name = consume(TemplateTokenType::Identifier, "Expected function name after fn");
    if (is_reserved_name(name.lexeme)) {
        throw DiagnosticError(make_error(name, "Reserved function name: " + name.lexeme));
    }
    consume(TemplateTokenType::LeftParen, "Expected '(' after function name");

    std::vector<std::string> parameters;
    std::unordered_set<std::string> seen_parameters;
    if (!check(TemplateTokenType::RightParen)) {
        do {
            const TemplateToken parameter = consume(TemplateTokenType::Identifier, "Expected parameter name");
            if (is_reserved_name(parameter.lexeme)) {
                throw DiagnosticError(make_error(parameter, "Reserved parameter name: " + parameter.lexeme));
            }
            if (!seen_parameters.insert(parameter.lexeme).second) {
                throw DiagnosticError(make_error(parameter, "Duplicate parameter name: " + parameter.lexeme));
            }
            parameters.push_back(parameter.lexeme);
        } while (match(TemplateTokenType::Comma));
    }
    consume(TemplateTokenType::RightParen, "Expected ')' after function parameters");

    FunctionMode mode = FunctionMode::Template;
    if (match(TemplateTokenType::KeywordLuaBlock)) {
        mode = FunctionMode::Lua;
    }

    consume(TemplateTokenType::TagClose, mode == FunctionMode::Lua
        ? "Expected tag end after function lua:block header"
        : "Expected tag end after function header");

    auto node = std::make_unique<FunctionDefNode>(name.lexeme, std::move(parameters), mode, start.span);
    if (mode == FunctionMode::Lua) {
        node->lua_source = parse_raw_body_until(TemplateTokenType::KeywordEndFn);
    } else {
        node->body = parse_nodes_until({TemplateTokenType::KeywordEndFn});
    }

    consume(TemplateTokenType::TagOpen, "Expected tag start before endfn");
    consume(TemplateTokenType::KeywordEndFn, "Expected endfn keyword");
    const TemplateToken end = consume(TemplateTokenType::TagClose, "Expected tag end after endfn");
    node->span.end = end.span.end;
    apply_trim_flags(*node, start, end);
    return node;
}

std::unique_ptr<InterpolationNode> TemplateParser::parse_interpolation() {
    const TemplateToken start = consume(TemplateTokenType::TagOpen, "Expected tag start");
    auto expression = parse_expression();
    const TemplateToken end = consume(TemplateTokenType::TagClose, "Expected tag end after expression");

    SourceSpan span = start.span;
    span.end = end.span.end;
    auto node = std::make_unique<InterpolationNode>(std::move(expression), span);
    apply_trim_flags(*node, start, end);
    return node;
}

std::unique_ptr<ExpressionNode> TemplateParser::parse_expression() {
    return parse_pipe();
}

std::unique_ptr<ExpressionNode> TemplateParser::parse_pipe() {
    auto expression = parse_or();
    while (match(TemplateTokenType::Pipe)) {
        const TemplateToken filter = consume(TemplateTokenType::Identifier, "Expected filter name after '|'");
        std::vector<std::unique_ptr<ExpressionNode>> arguments;
        SourceSpan span = expression->span;
        span.end = filter.span.end;
        if (match(TemplateTokenType::LeftParen)) {
            if (!check(TemplateTokenType::RightParen)) {
                do {
                    arguments.push_back(parse_expression());
                } while (match(TemplateTokenType::Comma));
            }
            const TemplateToken end = consume(TemplateTokenType::RightParen, "Expected ')' after filter arguments");
            span.end = end.span.end;
        }
        expression = std::make_unique<FilterCallExpr>(std::move(expression), filter.lexeme, std::move(arguments), span);
    }
    return expression;
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
    auto expression = parse_comparison();
    while (match(TemplateTokenType::EqualEqual) || match(TemplateTokenType::BangEqual)) {
        const TemplateToken op = tokens_[current_ - 1];
        auto right = parse_comparison();
        SourceSpan span = expression->span;
        span.end = right->span.end;
        expression = std::make_unique<BinaryExpr>(std::move(expression), op.lexeme, std::move(right), span);
    }
    return expression;
}

std::unique_ptr<ExpressionNode> TemplateParser::parse_comparison() {
    auto expression = parse_unary();
    if (match(TemplateTokenType::Less) || match(TemplateTokenType::Greater)
        || match(TemplateTokenType::LessEqual) || match(TemplateTokenType::GreaterEqual)
        || match(TemplateTokenType::KeywordIn)) {
        const TemplateToken op = tokens_[current_ - 1];
        auto right = parse_unary();
        SourceSpan span = expression->span;
        span.end = right->span.end;
        expression = std::make_unique<BinaryExpr>(std::move(expression), op.lexeme, std::move(right), span);
        if (check(TemplateTokenType::Less) || check(TemplateTokenType::Greater)
            || check(TemplateTokenType::LessEqual) || check(TemplateTokenType::GreaterEqual)
            || check(TemplateTokenType::KeywordIn)) {
            throw DiagnosticError(make_error(peek(), "Chained comparison operators are not supported"));
        }
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
    return parse_postfix();
}

std::unique_ptr<ExpressionNode> TemplateParser::parse_postfix() {
    auto expression = parse_primary();

    while (true) {
        if (match(TemplateTokenType::Dot)) {
            const TemplateToken member = consume(TemplateTokenType::Identifier, "Expected member name after '.'");
            SourceSpan span = expression->span;
            span.end = member.span.end;
            expression = std::make_unique<MemberAccessExpr>(std::move(expression), member.lexeme, span);
            continue;
        }

        if (match(TemplateTokenType::LeftParen)) {
            if (expression->kind != ExpressionKind::Identifier) {
                throw DiagnosticError(make_error(tokens_[current_ - 1], "Function call requires identifier callee"));
            }
            const std::string name = static_cast<const IdentifierExpr&>(*expression).name;
            std::vector<std::unique_ptr<ExpressionNode>> arguments;
            if (!check(TemplateTokenType::RightParen)) {
                do {
                    arguments.push_back(parse_expression());
                } while (match(TemplateTokenType::Comma));
            }
            const TemplateToken end = consume(TemplateTokenType::RightParen, "Expected ')' after function arguments");
            SourceSpan span = expression->span;
            span.end = end.span.end;
            expression = std::make_unique<FunctionCallExpr>(name, std::move(arguments), span);
            continue;
        }

        if (match(TemplateTokenType::LeftBracket)) {
            auto index = parse_expression();
            const TemplateToken end = consume(TemplateTokenType::RightBracket, "Expected ']' after index expression");
            SourceSpan span = expression->span;
            span.end = end.span.end;
            expression = std::make_unique<IndexAccessExpr>(std::move(expression), std::move(index), span);
            continue;
        }

        break;
    }

    return expression;
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
        if (token.lexeme == "len" && match(TemplateTokenType::LeftParen)) {
            auto operand = parse_expression();
            const TemplateToken end = consume(TemplateTokenType::RightParen, "Expected ')' after len expression");
            SourceSpan span = token.span;
            span.end = end.span.end;
            return std::make_unique<LenCallExpr>(std::move(operand), span);
        }
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

std::string TemplateParser::parse_raw_body_until(TemplateTokenType terminator) {
    std::string source;
    while (!is_at_end()) {
        if (check(TemplateTokenType::TagOpen) && check(terminator, 1)) {
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

}
