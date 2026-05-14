#include "Engine.h"

#include "config/RuleResolver.h"
#include "io/InputReader.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "runtime/EngineRuntime.h"
#include "support/Diagnostic.h"

#include <chrono>
#include <unordered_map>

namespace prebyte {

namespace {

EffectiveSettings make_compile_settings(const CompileOptions& options) {
    EffectiveSettings settings;
    settings.variable_prefix = options.variable_prefix;
    settings.variable_suffix = options.variable_suffix;
    settings.replace_tabs = options.replace_tabs;
    settings.tab_size = options.tab_size;
    return settings;
}

Value own_context_value(Value value) {
    if (const auto string_value = value.try_as_string_view()) {
        return Value(std::string(*string_value));
    }
    return value;
}

Diagnostic make_api_error(const std::string& message) {
    Diagnostic diagnostic;
    diagnostic.code = "API001";
    diagnostic.message = message;
    return diagnostic;
}

std::filesystem::path render_path_for(const CompiledProgram& program) {
    if (!program.logical_path.empty()) {
        return program.logical_path;
    }
    return program.source_path;
}

}

struct CompiledTemplate::Impl {
    CompiledProgram program;
};

struct Engine::Impl {
    EngineRuntime runtime;
};

void RenderContext::set(std::string name, std::string value) {
    variables_[std::move(name)] = Value(std::move(value));
}

void RenderContext::set(std::string name, const char* value) {
    variables_[std::move(name)] = Value(value != nullptr ? std::string(value) : std::string());
}

void RenderContext::set(std::string name, Value value) {
    variables_[std::move(name)] = own_context_value(std::move(value));
}

void RenderContext::set_args(std::vector<std::string> args) {
    args_ = std::move(args);
}

CompiledTemplate::CompiledTemplate(std::shared_ptr<const Impl> impl)
    : impl_(std::move(impl)) {}

const std::filesystem::path& CompiledTemplate::source_path() const {
    static const std::filesystem::path empty_path;
    return impl_ ? impl_->program.source_path : empty_path;
}

const std::filesystem::path& CompiledTemplate::logical_path() const {
    static const std::filesystem::path empty_path;
    return impl_ ? impl_->program.logical_path : empty_path;
}

Engine::Engine()
    : impl_(std::make_shared<Impl>()) {}

CompiledTemplate Engine::compile(std::string_view source,
                                 std::filesystem::path source_path,
                                 std::filesystem::path logical_path,
                                 const CompileOptions& options) const {
    CompiledTemplateCompiler compiler;
    CompiledProgram program = compiler.compile_source(source, source_path, logical_path, make_compile_settings(options));
    return CompiledTemplate(std::make_shared<CompiledTemplate::Impl>(CompiledTemplate::Impl{std::move(program)}));
}

CompiledTemplate Engine::compile_file(const std::filesystem::path& path,
                                      const CompileOptions& options) const {
    InputReader reader;
    InputBuffer input = reader.read(std::optional<std::filesystem::path>(path));
    return compile(input.view(), path, path, options);
}

CompiledTemplate Engine::load_compiled_file(const std::filesystem::path& path) const {
    InputReader reader;
    InputBuffer input = reader.read(std::optional<std::filesystem::path>(path));
    CompiledTemplateSerializer serializer;
    CompiledProgram program = serializer.deserialize(input.view(), path);
    return CompiledTemplate(std::make_shared<CompiledTemplate::Impl>(CompiledTemplate::Impl{std::move(program)}));
}

std::string Engine::render(const CompiledTemplate& tpl,
                           const RenderContext& ctx,
                           const RenderOptions& opts) const {
    if (!tpl.impl_) {
        throw DiagnosticError(make_api_error("Compiled template is empty"));
    }

    const CompiledProgram& program = tpl.impl_->program;
    std::string output;
    output.reserve(program.data_blob.size());
    render_to(tpl, [&output](std::string_view chunk) {
        output.append(chunk.data(), chunk.size());
    }, ctx, opts);
    return output;
}

void Engine::render_to(const CompiledTemplate& tpl,
                       ChunkSink sink,
                       const RenderContext& ctx,
                       const RenderOptions& opts) const {
    if (!tpl.impl_) {
        throw DiagnosticError(make_api_error("Compiled template is empty"));
    }
    if (!sink) {
        throw DiagnosticError(make_api_error("Render sink is empty"));
    }

    const CompiledProgram& program = tpl.impl_->program;
    const std::filesystem::path current_file = render_path_for(program);

    std::map<std::filesystem::path, EffectiveSettings> effective_settings_cache;
    if (!current_file.empty()) {
        effective_settings_cache.emplace(current_file, opts.settings);
    }

    std::unordered_map<RenderSession::PreparedIncludeKey, RenderSession::PreparedIncludeEntry,
                       RenderSession::PreparedIncludeKeyHash> prepared_include_cache;

    RenderSession session;
    session.configuration.base_settings = opts.settings;
    session.variables.set_all(ctx.variables_);
    session.args = ctx.args_;
    session.effective_settings_cache_ref = &effective_settings_cache;
    session.prepared_include_cache_ref = &prepared_include_cache;
    session.start_time = std::chrono::steady_clock::now();

    impl_->runtime.renderer.render_program_to(program, opts.settings, current_file, session, sink);
}

}
