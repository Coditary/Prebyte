#include "template/ast/Expression.h"

namespace prebyte {

ExpressionNode::ExpressionNode(ExpressionKind kind, SourceSpan span)
    : kind(kind), span(std::move(span)) {}

IdentifierExpr::IdentifierExpr(std::string name, SourceSpan span)
    : ExpressionNode(ExpressionKind::Identifier, std::move(span)), name(std::move(name)) {}

StringExpr::StringExpr(std::string value, SourceSpan span)
    : ExpressionNode(ExpressionKind::String, std::move(span)), value(std::move(value)) {}

NumberExpr::NumberExpr(double value, std::string lexeme, SourceSpan span)
    : ExpressionNode(ExpressionKind::Number, std::move(span)), value(value), lexeme(std::move(lexeme)) {}

BoolExpr::BoolExpr(bool value, SourceSpan span)
    : ExpressionNode(ExpressionKind::Bool, std::move(span)), value(value) {}

UnaryExpr::UnaryExpr(std::string op, std::unique_ptr<ExpressionNode> operand, SourceSpan span)
    : ExpressionNode(ExpressionKind::Unary, std::move(span)), op(std::move(op)), operand(std::move(operand)) {}

BinaryExpr::BinaryExpr(std::unique_ptr<ExpressionNode> left, std::string op, std::unique_ptr<ExpressionNode> right,
                       SourceSpan span)
    : ExpressionNode(ExpressionKind::Binary, std::move(span)), left(std::move(left)), op(std::move(op)),
      right(std::move(right)) {}

GroupedExpr::GroupedExpr(std::unique_ptr<ExpressionNode> expression, SourceSpan span)
    : ExpressionNode(ExpressionKind::Grouped, std::move(span)), expression(std::move(expression)) {}

LuaCallExpr::LuaCallExpr(std::string source, SourceSpan span)
    : ExpressionNode(ExpressionKind::LuaCall, std::move(span)), source(std::move(source)) {}

}
