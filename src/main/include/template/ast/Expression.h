#pragma once

#include <memory>
#include <string>

#include "support/SourceSpan.h"

namespace prebyte {

enum class ExpressionKind {
    Identifier,
    String,
    Number,
    Bool,
    Unary,
    Binary,
    Grouped,
    LuaCall,
};

class ExpressionNode {
public:
    explicit ExpressionNode(ExpressionKind kind, SourceSpan span = {});
    virtual ~ExpressionNode() = default;

    ExpressionKind kind;
    SourceSpan span;
};

class IdentifierExpr final : public ExpressionNode {
public:
    IdentifierExpr(std::string name, SourceSpan span = {});

    std::string name;
};

class StringExpr final : public ExpressionNode {
public:
    StringExpr(std::string value, SourceSpan span = {});

    std::string value;
};

class NumberExpr final : public ExpressionNode {
public:
    NumberExpr(double value, std::string lexeme, SourceSpan span = {});

    double value;
    std::string lexeme;
};

class BoolExpr final : public ExpressionNode {
public:
    BoolExpr(bool value, SourceSpan span = {});

    bool value;
};

class UnaryExpr final : public ExpressionNode {
public:
    UnaryExpr(std::string op, std::unique_ptr<ExpressionNode> operand, SourceSpan span = {});

    std::string op;
    std::unique_ptr<ExpressionNode> operand;
};

class BinaryExpr final : public ExpressionNode {
public:
    BinaryExpr(std::unique_ptr<ExpressionNode> left, std::string op, std::unique_ptr<ExpressionNode> right,
               SourceSpan span = {});

    std::unique_ptr<ExpressionNode> left;
    std::string op;
    std::unique_ptr<ExpressionNode> right;
};

class GroupedExpr final : public ExpressionNode {
public:
    GroupedExpr(std::unique_ptr<ExpressionNode> expression, SourceSpan span = {});

    std::unique_ptr<ExpressionNode> expression;
};

class LuaCallExpr final : public ExpressionNode {
public:
    LuaCallExpr(std::string source, SourceSpan span = {});

    std::string source;
};

}
