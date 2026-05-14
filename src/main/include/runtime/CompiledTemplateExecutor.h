#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "config/ConfigTypes.h"
#include "runtime/CompiledTemplateProgram.h"
#include "runtime/FilterRegistry.h"
#include "runtime/IncludeResolver.h"
#include "runtime/LuaRuntime.h"
#include "runtime/RenderSession.h"
#include "runtime/ValueResolver.h"

namespace prebyte {

class RuleResolver;

class CompiledTemplateExecutor {
public:
    using ChunkSink = std::function<void(std::string_view)>;

    CompiledTemplateExecutor(const RuleResolver& rule_resolver, const IncludeResolver& include_resolver,
                             const BuiltinRegistry& builtins);

    void execute(const CompiledProgram& program, const EffectiveSettings& settings,
                 const std::filesystem::path& current_file, RenderSession& session,
                 const ChunkSink& sink) const;
    Value call_function(const RenderSession::FunctionDefinition& function,
                        std::vector<Value> arguments,
                        const EffectiveSettings& settings,
                        const std::filesystem::path& current_file,
                        RenderSession& session) const;

private:
    void execute_range(const CompiledProgram& program, std::size_t begin, std::size_t end,
                       const EffectiveSettings& settings, const std::filesystem::path& current_file,
                       RenderSession& session, const ChunkSink& sink, bool count_output) const;
    Value evaluate_expression(const CompiledProgram& program, InstructionRange range,
                              const EffectiveSettings& settings, const std::filesystem::path& current_file,
                              RenderSession& session) const;
    std::string_view data_view(const CompiledProgram& program, std::uint32_t offset, std::uint32_t length) const;
    SourceSpan make_span(const std::filesystem::path& current_file, std::uint32_t line, std::uint32_t column) const;

    const RuleResolver& rule_resolver_;
    const IncludeResolver& include_resolver_;
    FilterRegistry filters_;
    ValueResolver resolver_;
};

}
