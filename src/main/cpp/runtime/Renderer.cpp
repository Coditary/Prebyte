#include "runtime/Renderer.h"

#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateCache.h"
#include "runtime/FileMetadataCache.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "runtime/CompiledTemplateWriter.h"
#include "support/TextUtil.h"

namespace prebyte {

Renderer::Renderer(const RuleResolver& rule_resolver, const IncludeResolver& include_resolver,
                   const ExpressionEngine& expression_engine)
    : rule_resolver_(rule_resolver), include_resolver_(include_resolver),
      builtins_(static_cast<const ExpressionEvaluator&>(expression_engine).builtins()),
      executor_(rule_resolver, include_resolver, builtins_) {}

std::string Renderer::render_source(std::string_view source, const EffectiveSettings& settings,
                                    const std::filesystem::path& current_file, RenderSession& session) const {
    std::string output;
    output.reserve(source.size());
    render_source_to(source, settings, current_file, session, [&output](std::string_view chunk) {
        output.append(chunk.data(), chunk.size());
    });
    return output;
}

void Renderer::render_source_to(std::string_view source, const EffectiveSettings& settings,
                                const std::filesystem::path& current_file, RenderSession& session,
                                const ChunkSink& sink) const {
    const bool file_backed_source = !current_file.empty() && FileMetadataCache::instance().probe(current_file).exists;
    if (file_backed_source) {
        CompiledTemplateSerializer serializer;
        if (const CompiledProgram* compiled = serializer.try_load_valid(serializer.compiled_path_for_source(current_file), settings)) {
            render_program_to(*compiled, settings, compiled->logical_path, session, sink);
            return;
        }
    }

    std::string prepared;
    std::string_view active_source(source);
    if (settings.replace_tabs) {
        prepared = prepare_source(source, settings);
        active_source = prepared;
    }

    CompiledTemplateCompiler compiler;
    const CompiledProgram program = compiler.compile_source(active_source, current_file, current_file, settings);

    CompiledTemplateSerializer serializer;
    if (file_backed_source) {
        CompiledTemplateCache::instance().store_in_memory(serializer.compiled_path_for_source(current_file), program, settings);
        CompiledTemplateWriter::instance().enqueue(serializer.compiled_path_for_source(current_file),
                                                   serializer.serialize(program));
    }
    render_program_to(program, settings, current_file, session, sink);
}

void Renderer::render_program_to(const CompiledProgram& program, const EffectiveSettings& settings,
                                 const std::filesystem::path& current_file, RenderSession& session,
                                 const ChunkSink& sink) const {
    executor_.execute(program, settings, current_file, session, sink);
}

std::string Renderer::render_program(const CompiledProgram& program, const EffectiveSettings& settings,
                                     const std::filesystem::path& current_file, RenderSession& session) const {
    std::string output;
    output.reserve(program.data_blob.size());
    render_program_to(program, settings, current_file, session, [&output](std::string_view chunk) {
        output.append(chunk.data(), chunk.size());
    });
    return output;
}

std::string Renderer::prepare_source(std::string_view source, const EffectiveSettings& settings) const {
    if (!settings.replace_tabs) {
        return std::string(source);
    }
    return text::replace_tabs(std::string(source), settings.tab_size);
}

}
