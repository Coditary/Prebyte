#include "PrebyteEngine.h"

#include "config/ProfileMerger.h"
#include "config/RuleResolver.h"
#include "config/SettingsLoader.h"
#include "config/VariableDefinitionParser.h"
#include "io/InputReader.h"
#include "io/OutputWriter.h"
#include "runtime/compiled/CompiledTemplateCompiler.h"
#include "runtime/compiled/CompiledTemplateCache.h"
#include "runtime/cache/FileMetadataCache.h"
#include "runtime/compiled/CompiledTemplateSerializer.h"
#include "runtime/compiled/CompiledProgramAnalysis.h"
#include "runtime/render/EngineRuntime.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace prebyte {

namespace {

SettingsData load_settings_for_engine(const std::optional<std::filesystem::path>& settings_path) {
    if (!settings_path.has_value()) {
        return SettingsData{};
    }
    SettingsLoader loader;
    return loader.load(*settings_path);
}

ResolvedConfiguration resolve_engine_configuration(const SettingsData& settings,
                                                   const std::vector<std::string>& profile_names,
                                                   const std::vector<std::string>& rule_args,
                                                   const std::vector<std::string>& ignore_names,
                                                   const std::vector<std::filesystem::path>& include_paths) {
    ProfileMerger profile_merger;
    RuleResolver rule_resolver;
    SettingsData merged = profile_merger.merge(settings, profile_names);
    return rule_resolver.resolve(merged, rule_args, ignore_names, include_paths, false);
}

VariableContext resolve_engine_variables(const std::vector<std::string>& define_args,
                                         const ResolvedConfiguration& configuration) {
    VariableDefinitionParser parser;
    return parser.parse(define_args, configuration.variables, configuration.ignore_names);
}

}

struct Prebyte::PreparedState {
    struct CachedProgramRef {
        const CompiledProgram* program = nullptr;
        std::chrono::steady_clock::time_point valid_until = std::chrono::steady_clock::time_point::min();
    };

    struct CachedOutput {
        std::string output;
        std::chrono::steady_clock::time_point valid_until = std::chrono::steady_clock::time_point::min();
    };

    explicit PreparedState(ResolvedConfiguration configuration_in, VariableContext variable_context,
                           const std::vector<std::string>& render_args_in)
        : configuration(std::move(configuration_in)),
          render_args(render_args_in),
          ignore_names(std::move(variable_context.ignore_names)) {
        variables.set_all(variable_context.variables);
        variables.set_all(variable_context.structured_variables);
    }

    const EffectiveSettings& settings_for(const std::filesystem::path& file_path) {
        auto [it, inserted] = effective_settings_cache.try_emplace(file_path);
        if (inserted) {
            it->second = runtime.rule_resolver.resolve_for_file(configuration, file_path);
        }
        return it->second;
    }

    const CompiledProgram& inline_program_for(const std::string& source, const EffectiveSettings& settings) {
        if (const CompiledProgram* cached = CompiledTemplateCache::instance().find_inline(source, settings)) {
            return *cached;
        }
        CompiledTemplateCompiler compiler;
        const CompiledProgram* stored = CompiledTemplateCache::instance().store_inline(
            source, compiler.compile_source(source, {}, {}, settings), settings);
        return *stored;
    }

    const CompiledProgram* compiled_for(const std::filesystem::path& source_path,
                                        const EffectiveSettings& settings,
                                        const CompiledTemplateSerializer& serializer) {
        auto it = compiled_program_cache.find(source_path);
        if (it != compiled_program_cache.end() && std::chrono::steady_clock::now() < it->second.valid_until) {
            return it->second.program;
        }

        if (const CompiledProgram* program = serializer.try_load_valid(serializer.compiled_path_for_source(source_path), settings)) {
            compiled_program_cache[source_path] = CachedProgramRef{program, std::chrono::steady_clock::now() + FileMetadataCache::ttl()};
            return program;
        }
        compiled_program_cache.erase(source_path);
        return nullptr;
    }

    const std::string* cached_inline_output(const std::string& source) const {
        auto it = inline_output_cache.find(source);
        if (it == inline_output_cache.end()) {
            return nullptr;
        }
        return &it->second.output;
    }

    void store_inline_output(const std::string& source, std::string output) {
        inline_output_cache[source] = CachedOutput{std::move(output), std::chrono::steady_clock::time_point::max()};
    }

    const std::string* cached_file_output(const std::filesystem::path& source_path) const {
        auto it = file_output_cache.find(source_path);
        if (it == file_output_cache.end()) {
            return nullptr;
        }
        if (std::chrono::steady_clock::now() >= it->second.valid_until) {
            return nullptr;
        }
        return &it->second.output;
    }

    const std::string* cached_file_output_raw(const std::string& source_path) const {
        auto it = file_output_cache_raw.find(source_path);
        if (it == file_output_cache_raw.end()) {
            return nullptr;
        }
        if (std::chrono::steady_clock::now() >= it->second.valid_until) {
            return nullptr;
        }
        return &it->second.output;
    }

    void store_file_output(const std::filesystem::path& source_path, std::string output) {
        file_output_cache[source_path] = CachedOutput{std::move(output), std::chrono::steady_clock::now() + FileMetadataCache::ttl()};
    }

    void store_file_output_raw(const std::string& source_path, std::string output) {
        file_output_cache_raw[source_path] = CachedOutput{std::move(output), std::chrono::steady_clock::now() + FileMetadataCache::ttl()};
    }

    bool memoizable(const CompiledProgram& program, const EffectiveSettings& settings) const {
        if (!render_args.empty() || settings.allow_env) {
            return false;
        }
        std::unordered_set<const CompiledProgram*> visited;
        return memoizable_recursive(program, settings, visited);
    }

    bool memoizable_recursive(const CompiledProgram& program, const EffectiveSettings& settings,
                              std::unordered_set<const CompiledProgram*>& visited) const {
        if (!visited.insert(&program).second) {
            return true;
        }
        if (settings.allow_env || program_has_dynamic_ops(program)) {
            return false;
        }

        for (std::size_t index = 0; index < program.template_instructions.size(); ++index) {
            if (program.template_instructions[index].opcode != TemplateOpcode::Include) {
                continue;
            }
            auto it = prepared_include_cache.find(RenderSession::PreparedIncludeKey{&program, static_cast<std::uint32_t>(index)});
            if (it == prepared_include_cache.end() || it->second.program == nullptr || it->second.settings == nullptr) {
                return false;
            }
            if (!memoizable_recursive(*it->second.program, *it->second.settings, visited)) {
                return false;
            }
        }
        return true;
    }

    void prepare_render_session() {
        render_session.reset_for_render();
        render_session.configuration_ref = &configuration;
        render_session.variables_ref = &variables;
        render_session.args_ref = &render_args;
        render_session.ignore_names_ref = &ignore_names;
        render_session.effective_settings_cache_ref = &effective_settings_cache;
        render_session.prepared_include_cache_ref = &prepared_include_cache;
        render_session.start_time = std::chrono::steady_clock::now();
    }

    void invalidate_render_outputs() {
        inline_output_cache.clear();
        file_output_cache.clear();
        file_output_cache_raw.clear();
        inline_output_seen.clear();
        file_output_seen_raw.clear();
        for (auto& [key, entry] : prepared_include_cache) {
            static_cast<void>(key);
            entry.cached_output.reset();
            entry.cached_variables_revision = 0;
        }
    }

    void apply_define_arg(const std::string& define_arg) {
        VariableDefinitionParser parser;
        const VariableContext variable_context = parser.parse({define_arg}, configuration.variables, ignore_names);
        variables.set_all(variable_context.variables);
        variables.set_all(variable_context.structured_variables);
        invalidate_render_outputs();
    }

    std::string render_inline(const std::string& input) {
        const EffectiveSettings& settings = settings_for({});
        if (const std::string* cached = cached_inline_output(input)) {
            return *cached;
        }
        const bool should_cache_output = !inline_output_seen.insert(input).second;

        prepare_render_session();
        const CompiledProgram& program = inline_program_for(input, settings);
        std::string output = runtime.renderer.render_program(program, settings, {}, render_session);
        if (should_cache_output && memoizable(program, settings)) {
            store_inline_output(input, output);
        }
        return output;
    }

    std::string render_file(const std::string& file_path) {
        if (const std::string* cached = cached_file_output_raw(file_path)) {
            return *cached;
        }
        const bool should_cache_output = !file_output_seen_raw.insert(file_path).second;
        const std::filesystem::path path(file_path);
        if (const std::string* cached = cached_file_output(path)) {
            return *cached;
        }

        const EffectiveSettings& effective_settings = settings_for(path);

        prepare_render_session();
        render_session.include_anchor_root = path.parent_path();

        CompiledTemplateSerializer serializer;
        if (path.extension() == ".pbc") {
            InputReader reader;
            const InputBuffer input = reader.read(path);
            const CompiledProgram program = serializer.deserialize(input.view(), path);
            return runtime.renderer.render_program(program, effective_settings, program.logical_path, render_session);
        }

        if (const CompiledProgram* compiled = compiled_for(path, effective_settings, serializer)) {
            std::string output = runtime.renderer.render_program(*compiled, effective_settings, compiled->logical_path, render_session);
            if (should_cache_output && memoizable(*compiled, effective_settings)) {
                store_file_output(path, output);
                store_file_output_raw(file_path, output);
            }
            return output;
        }

        InputReader reader;
        const InputBuffer input = reader.read(path);
        std::string output = runtime.renderer.render_source(input.view(), effective_settings, path, render_session);
        if (const CompiledProgram* compiled = compiled_for(path, effective_settings, serializer)) {
            if (should_cache_output && memoizable(*compiled, effective_settings)) {
                store_file_output(path, output);
                store_file_output_raw(file_path, output);
            }
        }
        return output;
    }

    ResolvedConfiguration configuration;
    VariableStore variables;
    const std::vector<std::string>& render_args;
    std::set<std::string> ignore_names;
    EngineRuntime runtime;
    std::map<std::filesystem::path, EffectiveSettings> effective_settings_cache;
    std::map<std::filesystem::path, CachedProgramRef> compiled_program_cache;
    std::map<std::string, CachedOutput, std::less<>> inline_output_cache;
    std::map<std::filesystem::path, CachedOutput> file_output_cache;
    std::map<std::string, CachedOutput, std::less<>> file_output_cache_raw;
    std::unordered_set<std::string> inline_output_seen;
    std::unordered_set<std::string> file_output_seen_raw;
    std::unordered_map<RenderSession::PreparedIncludeKey, RenderSession::PreparedIncludeEntry,
                       RenderSession::PreparedIncludeKeyHash> prepared_include_cache;
    RenderSession render_session;
};

Prebyte::Prebyte() = default;

Prebyte::Prebyte(std::string settings_file) {
    if (!settings_file.empty()) {
        settings_path_ = std::move(settings_file);
    }
}

Prebyte::~Prebyte() = default;

void Prebyte::set_variable(const std::string& name, const std::string& value) {
    std::lock_guard lock(mutex_);
    define_args_.push_back(name + '=' + value);
    if (prepared_) {
        prepared_->apply_define_arg(name + '=' + value);
    }
}

void Prebyte::set_variable(const std::string& name) {
    std::lock_guard lock(mutex_);
    define_args_.push_back(name + '=');
    if (prepared_) {
        prepared_->apply_define_arg(name + '=');
    }
}

void Prebyte::add_argument(const std::string& value) {
    std::lock_guard lock(mutex_);
    invalidate();
    render_args_.push_back(value);
}

void Prebyte::add_include_path(const std::string& path) {
    std::lock_guard lock(mutex_);
    invalidate();
    include_paths_.push_back(path);
}

void Prebyte::set_profile(const std::string& profile_name) {
    std::lock_guard lock(mutex_);
    invalidate();
    profile_names_.push_back(profile_name);
}

void Prebyte::set_ignore(const std::string& ignore_item) {
    std::lock_guard lock(mutex_);
    invalidate();
    ignore_names_.push_back(ignore_item);
}

void Prebyte::set_rule(const std::string& rule_name, const std::string& rule_value) {
    std::lock_guard lock(mutex_);
    invalidate();
    rule_args_.push_back(rule_name + '=' + rule_value);
}

std::string Prebyte::process(const std::string& input) const {
    std::lock_guard lock(mutex_);
    return prepared_state().render_inline(input);
}

std::string Prebyte::process_file(const std::string& file_path) const {
    std::lock_guard lock(mutex_);
    return prepared_state().render_file(file_path);
}

void Prebyte::process(const std::string& input, const std::string& output_path) const {
    std::lock_guard lock(mutex_);
    PreparedState& state = prepared_state();
    const EffectiveSettings& settings = state.settings_for({});
    OutputWriter writer;
    writer.write(state.render_inline(input), output_path, settings.output_encoding);
}

void Prebyte::process_file(const std::string& file_path, const std::string& output_path) const {
    std::lock_guard lock(mutex_);
    PreparedState& state = prepared_state();
    const EffectiveSettings& settings = state.settings_for(file_path);
    OutputWriter writer;
    writer.write(state.render_file(file_path), output_path, settings.output_encoding);
}

Command Prebyte::make_command() const {
    Command command;
    command.mode = CommandMode::Render;
    command.settings_path = settings_path_;
    command.render_args = render_args_;
    command.include_paths = include_paths_;
    command.define_args = define_args_;
    command.profile_names = profile_names_;
    command.ignore_names = ignore_names_;
    command.rule_args = rule_args_;
    return command;
}

void Prebyte::invalidate() const {
    prepared_.reset();
}

Prebyte::PreparedState& Prebyte::prepared_state() const {
    if (!prepared_) {
        const SettingsData settings = load_settings_for_engine(settings_path_);
        ResolvedConfiguration configuration = resolve_engine_configuration(settings, profile_names_, rule_args_, ignore_names_, include_paths_);
        VariableContext variable_context = resolve_engine_variables(define_args_, configuration);
        prepared_ = std::make_unique<PreparedState>(std::move(configuration), std::move(variable_context), render_args_);
    }
    return *prepared_;
}

}
