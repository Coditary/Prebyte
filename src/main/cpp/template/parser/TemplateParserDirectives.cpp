#include "template/parser/TemplateParser.h"

#include <unordered_set>

#include "template/parser/TemplateParserInternals.h"

namespace prebyte {

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

}
