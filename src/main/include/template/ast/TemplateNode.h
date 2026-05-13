#pragma once

#include <memory>
#include <string>
#include <vector>

#include "support/SourceSpan.h"
#include "template/ast/Expression.h"

namespace prebyte {

enum class TemplateNodeKind {
    Document,
    Text,
    Interpolation,
    Include,
    If,
    LuaExpr,
    LuaBlock,
};

class TemplateNode {
public:
    explicit TemplateNode(TemplateNodeKind kind, SourceSpan span = {});
    virtual ~TemplateNode() = default;

    TemplateNodeKind kind;
    SourceSpan span;
};

using TemplateNodePtr = std::unique_ptr<TemplateNode>;

class DocumentNode final : public TemplateNode {
public:
    DocumentNode();

    std::vector<TemplateNodePtr> children;
};

class TextNode final : public TemplateNode {
public:
    TextNode(std::string text, SourceSpan span = {});

    std::string text;
};

class InterpolationNode final : public TemplateNode {
public:
    InterpolationNode(std::unique_ptr<ExpressionNode> expression, SourceSpan span = {});

    std::unique_ptr<ExpressionNode> expression;
};

class IncludeNode final : public TemplateNode {
public:
    IncludeNode(std::string path, SourceSpan span = {});

    std::string path;
};

struct IfBranch {
    std::unique_ptr<ExpressionNode> condition;
    std::vector<TemplateNodePtr> body;
    SourceSpan span;
};

class IfNode final : public TemplateNode {
public:
    explicit IfNode(SourceSpan span = {});

    std::vector<IfBranch> branches;
};

class LuaExprNode final : public TemplateNode {
public:
    LuaExprNode(std::string source, SourceSpan span = {});

    std::string source;
};

class LuaBlockNode final : public TemplateNode {
public:
    LuaBlockNode(std::string source, SourceSpan span = {});

    std::string source;
};

}
