#include "template/parser/TemplateParser.h"

#include "template/parser/TemplateParserInternals.h"

namespace prebyte {

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

}
