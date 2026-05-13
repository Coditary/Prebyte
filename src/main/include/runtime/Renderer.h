#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "config/ConfigTypes.h"
#include "config/RuleResolver.h"
#include "runtime/ExpressionEngine.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/IncludeResolver.h"
#include "runtime/RenderSession.h"
#include "template/ast/TemplateNode.h"

namespace prebyte {

class Renderer {
public:
    Renderer(const RuleResolver& rule_resolver, const IncludeResolver& include_resolver,
             const ExpressionEngine& expression_engine);

    std::string render_source(std::string_view source, const EffectiveSettings& settings,
                              const std::filesystem::path& current_file, RenderSession& session) const;

private:
    void render_source_into(std::string_view source, const EffectiveSettings& settings,
                            const std::filesystem::path& current_file, RenderSession& session,
                            std::string& output) const;
    void render_document(const DocumentNode& document, const EffectiveSettings& settings,
                         const std::filesystem::path& current_file, RenderSession& session,
                         std::string& output) const;
    void render_node(const TemplateNode& node, const EffectiveSettings& settings,
                     const std::filesystem::path& current_file, RenderSession& session,
                     std::string& output) const;
    std::string render_interpolation(const InterpolationNode& node, const EffectiveSettings& settings,
                                     const std::filesystem::path& current_file, RenderSession& session) const;
    std::string render_lua_expression(const LuaExprNode& node, const EffectiveSettings& settings,
                                      const std::filesystem::path& current_file, RenderSession& session) const;
    std::string render_lua_block(const LuaBlockNode& node, const EffectiveSettings& settings,
                                 const std::filesystem::path& current_file, RenderSession& session) const;
    std::string prepare_source(std::string_view source, const EffectiveSettings& settings) const;

    const RuleResolver& rule_resolver_;
    const IncludeResolver& include_resolver_;
    const ExpressionEngine& expression_engine_;
};

}
