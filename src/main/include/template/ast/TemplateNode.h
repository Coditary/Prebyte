#pragma once

#include <memory>
#include <optional>
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
    For,
    Set,
    FunctionDef,
    LuaExpr,
    LuaBlock,
};

class TemplateNode {
public:
    explicit TemplateNode(TemplateNodeKind kind, SourceSpan span = {});
    virtual ~TemplateNode() = default;

    TemplateNodeKind kind;
    SourceSpan span;
    bool trim_left = false;
    bool trim_right = false;
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

class ForNode final : public TemplateNode {
public:
    ForNode(std::string value_name, std::unique_ptr<ExpressionNode> iterable,
            std::optional<std::string> key_name, SourceSpan span = {});

    std::string value_name;
    std::optional<std::string> key_name;
    std::unique_ptr<ExpressionNode> iterable;
    std::vector<TemplateNodePtr> body;
    std::vector<TemplateNodePtr> else_body;
};

class SetNode final : public TemplateNode {
public:
    SetNode(std::string name, std::unique_ptr<ExpressionNode> expression, SourceSpan span = {});

    std::string name;
    std::unique_ptr<ExpressionNode> expression;
};

enum class FunctionMode {
    Template,
    Lua,
};

class FunctionDefNode final : public TemplateNode {
public:
    FunctionDefNode(std::string name, std::vector<std::string> parameters,
                    FunctionMode mode, SourceSpan span = {});

    std::string name;
    std::vector<std::string> parameters;
    FunctionMode mode = FunctionMode::Template;
    std::vector<TemplateNodePtr> body;
    std::string lua_source;
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
