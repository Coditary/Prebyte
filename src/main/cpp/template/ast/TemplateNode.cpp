#include "template/ast/TemplateNode.h"

namespace prebyte {

TemplateNode::TemplateNode(TemplateNodeKind kind, SourceSpan span)
    : kind(kind), span(std::move(span)) {}

DocumentNode::DocumentNode()
    : TemplateNode(TemplateNodeKind::Document, {}) {}

TextNode::TextNode(std::string text, SourceSpan span)
    : TemplateNode(TemplateNodeKind::Text, std::move(span)), text(std::move(text)) {}

InterpolationNode::InterpolationNode(std::unique_ptr<ExpressionNode> expression, SourceSpan span)
    : TemplateNode(TemplateNodeKind::Interpolation, std::move(span)), expression(std::move(expression)) {}

IncludeNode::IncludeNode(std::string path, SourceSpan span)
    : TemplateNode(TemplateNodeKind::Include, std::move(span)), path(std::move(path)) {}

IfNode::IfNode(SourceSpan span)
    : TemplateNode(TemplateNodeKind::If, std::move(span)) {}

LuaExprNode::LuaExprNode(std::string source, SourceSpan span)
    : TemplateNode(TemplateNodeKind::LuaExpr, std::move(span)), source(std::move(source)) {}

LuaBlockNode::LuaBlockNode(std::string source, SourceSpan span)
    : TemplateNode(TemplateNodeKind::LuaBlock, std::move(span)), source(std::move(source)) {}

}
