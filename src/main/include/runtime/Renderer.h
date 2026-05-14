#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "config/ConfigTypes.h"
#include "config/RuleResolver.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/CompiledTemplateExecutor.h"
#include "runtime/ExpressionEngine.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/IncludeResolver.h"
#include "runtime/RenderSession.h"

namespace prebyte {

class Renderer {
public:
    using ChunkSink = CompiledTemplateExecutor::ChunkSink;

    Renderer(const RuleResolver& rule_resolver, const IncludeResolver& include_resolver,
             const ExpressionEngine& expression_engine);

    std::string render_source(std::string_view source, const EffectiveSettings& settings,
                              const std::filesystem::path& current_file, RenderSession& session) const;
    std::string render_program(const CompiledProgram& program, const EffectiveSettings& settings,
                               const std::filesystem::path& current_file, RenderSession& session) const;
    void render_source_to(std::string_view source, const EffectiveSettings& settings,
                          const std::filesystem::path& current_file, RenderSession& session,
                          const ChunkSink& sink) const;
    void render_program_to(const CompiledProgram& program, const EffectiveSettings& settings,
                           const std::filesystem::path& current_file, RenderSession& session,
                           const ChunkSink& sink) const;

private:
    std::string prepare_source(std::string_view source, const EffectiveSettings& settings) const;

    const RuleResolver& rule_resolver_;
    const IncludeResolver& include_resolver_;
    const BuiltinRegistry& builtins_;
    CompiledTemplateExecutor executor_;
};

}
