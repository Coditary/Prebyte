#pragma once

#include <memory>
#include <vector>

#include "support/Diagnostic.h"
#include "template/ast/TemplateNode.h"
#include "template/lexer/TemplateToken.h"
#include "template/parser/TemplateParserOptions.h"

namespace prebyte {

// Recursive-descent parser for template tokens produced by TemplateLexer.
//
// Top-level flow:
//   parse_document() -> parse_nodes_until() -> parse_node() / parse_tag()
//
// Tags dispatch to directive parsers (if/for/set/fn/...) or fall back to interpolation.
// Expressions use a Pratt-style precedence chain (pipe -> or -> and -> ... -> primary).
class TemplateParser {
public:
    explicit TemplateParser(std::vector<TemplateToken> tokens, TemplateParserOptions options = {});

    std::unique_ptr<DocumentNode> parse_document();

private:
    // Token stream cursor
    const TemplateToken& peek(std::size_t offset = 0) const;
    bool is_at_end() const;
    bool check(TemplateTokenType type, std::size_t offset = 0) const;
    bool match(TemplateTokenType type);
    const TemplateToken& advance();
    const TemplateToken& consume(TemplateTokenType type, const std::string& message);
    bool is_terminator_ahead(const std::vector<TemplateTokenType>& terminators) const;
    Diagnostic make_error(const TemplateToken& token, const std::string& message) const;

    // Document / node structure
    std::vector<TemplateNodePtr> parse_nodes_until(const std::vector<TemplateTokenType>& terminators);
    TemplateNodePtr parse_node();
    TemplateNodePtr parse_tag();
    std::unique_ptr<InterpolationNode> parse_interpolation();
    std::string parse_raw_body_until(TemplateTokenType terminator);

    // Directives
    std::unique_ptr<IncludeNode> parse_include();
    std::unique_ptr<IfNode> parse_if_block();
    std::unique_ptr<ForNode> parse_for_block();
    std::unique_ptr<SetNode> parse_set_statement();
    std::unique_ptr<FunctionDefNode> parse_function_definition();
    std::unique_ptr<ExpressionNode> parse_if_condition(const std::string& branch_name);
    std::unique_ptr<LuaExprNode> parse_lua_expr();
    std::unique_ptr<LuaBlockNode> parse_lua_block();

    // Expression precedence chain (lowest to highest binding)
    void push_expression_depth();
    void pop_expression_depth();
    std::unique_ptr<ExpressionNode> parse_expression();
    std::unique_ptr<ExpressionNode> parse_pipe();
    std::unique_ptr<ExpressionNode> parse_or();
    std::unique_ptr<ExpressionNode> parse_and();
    std::unique_ptr<ExpressionNode> parse_equality();
    std::unique_ptr<ExpressionNode> parse_comparison();
    std::unique_ptr<ExpressionNode> parse_unary();
    std::unique_ptr<ExpressionNode> parse_postfix();
    std::unique_ptr<ExpressionNode> parse_primary();

    std::vector<TemplateToken> tokens_;
    std::size_t current_ = 0;
    std::size_t expression_depth_ = 0;
    TemplateParserOptions options_;
};

}
