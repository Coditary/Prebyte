#include "runtime/Renderer.h"

#include "runtime/LuaRuntime.h"
#include "support/Diagnostic.h"
#include "support/TextUtil.h"
#include "template/lexer/TemplateLexer.h"
#include "template/parser/TemplateParser.h"

namespace prebyte {

Renderer::Renderer(const RuleResolver& rule_resolver, const IncludeResolver& include_resolver,
                   const ExpressionEngine& expression_engine)
    : rule_resolver_(rule_resolver), include_resolver_(include_resolver), expression_engine_(expression_engine) {}

std::string Renderer::render_source(std::string_view source, const EffectiveSettings& settings,
                                    const std::filesystem::path& current_file, RenderSession& session) const {
    std::string output;
    output.reserve(source.size());
    render_source_into(source, settings, current_file, session, output);
    return output;
}

void Renderer::render_source_into(std::string_view source, const EffectiveSettings& settings,
                                  const std::filesystem::path& current_file, RenderSession& session,
                                  std::string& output) const {
    std::string prepared;
    std::string_view active_source(source);
    if (settings.replace_tabs) {
        prepared = prepare_source(source, settings);
        active_source = prepared;
    }

    TemplateLexer lexer(active_source, current_file.string(), settings.variable_prefix, settings.variable_suffix);
    TemplateParser parser(lexer.lex());
    std::unique_ptr<DocumentNode> document = parser.parse_document();
    render_document(*document, settings, current_file, session, output);
}

void Renderer::render_document(const DocumentNode& document, const EffectiveSettings& settings,
                               const std::filesystem::path& current_file, RenderSession& session,
                               std::string& output) const {
    for (const auto& child : document.children) {
        render_node(*child, settings, current_file, session, output);
    }
}

void Renderer::render_node(const TemplateNode& node, const EffectiveSettings& settings,
                           const std::filesystem::path& current_file, RenderSession& session,
                           std::string& output) const {
    switch (node.kind) {
    case TemplateNodeKind::Text:
        output.append(static_cast<const TextNode&>(node).text);
        return;
    case TemplateNodeKind::Interpolation:
        output += render_interpolation(static_cast<const InterpolationNode&>(node), settings, current_file, session);
        return;
    case TemplateNodeKind::LuaExpr:
        output += render_lua_expression(static_cast<const LuaExprNode&>(node), settings, current_file, session);
        return;
    case TemplateNodeKind::LuaBlock:
        output += render_lua_block(static_cast<const LuaBlockNode&>(node), settings, current_file, session);
        return;
    case TemplateNodeKind::Include: {
        if (!settings.allow_includes) {
            Diagnostic diagnostic;
            diagnostic.code = "RUNTIME003";
            diagnostic.message = "Includes are disabled";
            diagnostic.span = node.span;
            throw DiagnosticError(diagnostic);
        }
        ResolvedInclude include = include_resolver_.load(static_cast<const IncludeNode&>(node).path, current_file, settings, session);
        const EffectiveSettings include_settings = rule_resolver_.resolve_for_file(session.configuration, include.path);
        render_source_into(include.source.view(), include_settings, include.path, session, output);
        include_resolver_.pop(session);
        return;
    }
    case TemplateNodeKind::If: {
        const auto& if_node = static_cast<const IfNode&>(node);
        for (const IfBranch& branch : if_node.branches) {
            if (branch.condition && !expression_engine_.evaluate(*branch.condition, settings, session, current_file).to_bool()) {
                continue;
            }

            for (const auto& child : branch.body) {
                render_node(*child, settings, current_file, session, output);
            }
            return;
        }
        return;
    }
    case TemplateNodeKind::Document:
        render_document(static_cast<const DocumentNode&>(node), settings, current_file, session, output);
        return;
    }
}

std::string Renderer::render_interpolation(const InterpolationNode& node, const EffectiveSettings& settings,
                                           const std::filesystem::path& current_file, RenderSession& session) const {
    return expression_engine_.evaluate(*node.expression, settings, session, current_file).to_string();
}

std::string Renderer::render_lua_expression(const LuaExprNode& node, const EffectiveSettings& settings,
                                            const std::filesystem::path& current_file, RenderSession& session) const {
    if (!session.lua_runtime) {
        session.lua_runtime = std::make_shared<LuaRuntime>();
    }
    return session.lua_runtime->execute(node.source, LuaChunkMode::InlineValue, settings, session, current_file, node.span).to_string();
}

std::string Renderer::render_lua_block(const LuaBlockNode& node, const EffectiveSettings& settings,
                                       const std::filesystem::path& current_file, RenderSession& session) const {
    if (!session.lua_runtime) {
        session.lua_runtime = std::make_shared<LuaRuntime>();
    }
    return session.lua_runtime->execute(node.source, LuaChunkMode::BlockValue, settings, session, current_file, node.span).to_string();
}

std::string Renderer::prepare_source(std::string_view source, const EffectiveSettings& settings) const {
    if (!settings.replace_tabs) {
        return std::string(source);
    }
    return text::replace_tabs(std::string(source), settings.tab_size);
}

}
