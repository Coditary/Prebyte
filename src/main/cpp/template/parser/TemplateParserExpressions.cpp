#include "template/parser/TemplateParser.h"

namespace prebyte {

std::unique_ptr<ExpressionNode> TemplateParser::parse_expression() {
    push_expression_depth();
    auto expression = parse_pipe();
    pop_expression_depth();
    return expression;
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

}
