#include "app/BatchProcessor.h"

#include "app/AppRunner.h"
#include "config/ProfileMerger.h"
#include "config/RuleResolver.h"
#include "config/SettingsLoader.h"
#include "config/VariableDefinitionParser.h"
#include "datatypes/Data.h"
#include "io/InputBuffer.h"
#include "io/InputReader.h"
#include "io/OutputWriter.h"
#include "parser/JsonParser.h"
#include "runtime/expression/BuiltinRegistry.h"
#include "runtime/compiled/CompiledTemplateSerializer.h"
#include "runtime/resolution/IncludeResolver.h"
#include "runtime/render/Renderer.h"
#include "support/Diagnostic.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

namespace prebyte {

namespace {

Diagnostic make_batch_error(const std::string& message) {
    Diagnostic diagnostic;
    diagnostic.code = "BATCH001";
    diagnostic.message = message;
    return diagnostic;
}

std::string read_stdin() {
    std::ostringstream stream;
    stream << std::cin.rdbuf();
    return stream.str();
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw DiagnosticError(make_batch_error("Cannot read batch file: " + path.string()));
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool is_output_directory(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }
    const std::string text = path.string();
    if (!text.empty() && (text.back() == '/' || text.back() == '\\')) {
        return true;
    }
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        return std::filesystem::is_directory(path, error);
    }
    return path.extension().empty();
}

std::string data_scalar_to_string(const Data& data) {
    if (data.is_null()) {
        return {};
    }
    if (data.is_string() || data.is_bool() || data.is_int() || data.is_double()) {
        return data.as_string();
    }
    throw DiagnosticError(make_batch_error("Batch variable values must be scalars, objects, or arrays"));
}

struct BatchEntry {
    std::string output_name;
    Data::Map variables;
};

std::vector<BatchEntry> parse_batch_entries(const Data& root) {
    std::vector<BatchEntry> entries;
    if (root.is_array()) {
        const Data::Array& items = root.as_array();
        if (items.empty()) {
            throw DiagnosticError(make_batch_error("Batch input must contain at least one entry"));
        }
        entries.reserve(items.size());
        for (std::size_t index = 0; index < items.size(); ++index) {
            const Data& item = items[index];
            if (!item.is_map()) {
                throw DiagnosticError(make_batch_error("Batch array entries must be JSON objects"));
            }
            entries.push_back(BatchEntry{.output_name = std::to_string(index), .variables = item.as_map()});
        }
        return entries;
    }

    if (root.is_map()) {
        const Data::Map& items = root.as_map();
        if (items.empty()) {
            throw DiagnosticError(make_batch_error("Batch input must contain at least one entry"));
        }
        entries.reserve(items.size());
        for (const auto& [name, item] : items) {
            if (!item.is_map()) {
                throw DiagnosticError(make_batch_error("Batch object values must be JSON objects"));
            }
            entries.push_back(BatchEntry{.output_name = name, .variables = item.as_map()});
        }
        return entries;
    }

    throw DiagnosticError(make_batch_error("Batch input must be a JSON array or object"));
}

std::optional<std::string> extract_output_override(Data::Map& variables) {
    for (const char* key : {"$output", "_output"}) {
        const auto it = variables.find(key);
        if (it == variables.end()) {
            continue;
        }
        if (!it->second.is_string()) {
            throw DiagnosticError(make_batch_error("Batch output name override must be a string"));
        }
        const std::string output_name = it->second.as_string();
        variables.erase(it);
        return output_name;
    }
    return std::nullopt;
}

void apply_batch_variables(const Data::Map& variables, VariableStore& store) {
    for (const auto& [name, value] : variables) {
        if (value.is_map() || value.is_array()) {
            store.set_value(name, Value::from_data(value));
            continue;
        }
        store.set(name, data_scalar_to_string(value));
    }
}

std::string resolve_output_filename(const BatchEntry& entry, const std::optional<std::string>& override_name,
                                    const std::filesystem::path& template_path) {
    if (override_name.has_value()) {
        return *override_name;
    }
    if (!entry.output_name.empty()) {
        if (entry.output_name.contains('.') || template_path.empty()) {
            return entry.output_name;
        }
        return entry.output_name + template_path.extension().string();
    }
    return "output" + template_path.extension().string();
}

struct SharedTemplateInput {
    InputBuffer buffer;
    bool from_compiled_program = false;
    CompiledProgram compiled_program;
};

SharedTemplateInput load_template_input(const Command& command, const EffectiveSettings& effective_settings) {
    SharedTemplateInput shared;
    CompiledTemplateSerializer serializer;
    if (command.input_path.has_value() && command.input_path->extension() != ".pbc") {
        if (const CompiledProgram* compiled = serializer.try_load_valid(serializer.compiled_path_for_source(*command.input_path), effective_settings)) {
            shared.from_compiled_program = true;
            shared.compiled_program = *compiled;
            return shared;
        }
    }

    InputReader reader;
    shared.buffer = command.inline_input.has_value()
        ? InputBuffer::from_owned(*command.inline_input)
        : reader.read(command.input_path);
    if (command.input_path.has_value() && command.input_path->extension() == ".pbc") {
        shared.from_compiled_program = true;
        shared.compiled_program = serializer.deserialize(shared.buffer.view(), *command.input_path);
    }
    return shared;
}

RenderReport render_with_batch_variables(const Command& command, const SharedTemplateInput& shared_template,
                                         const Data::Map& batch_variables, const ResolvedConfiguration& configuration,
                                         const EffectiveSettings& effective_settings,
                                         const VariableContext& base_variable_context) {
    const auto start_time = std::chrono::steady_clock::now();
    VariableStore variable_store;
    variable_store.set_all(base_variable_context.variables);
    variable_store.set_all(base_variable_context.structured_variables);
    apply_batch_variables(batch_variables, variable_store);

    std::map<std::filesystem::path, EffectiveSettings> effective_settings_cache;
    effective_settings_cache.emplace(command.input_path.value_or(std::filesystem::path{}), effective_settings);

    RenderSession session;
    session.configuration_ref = &configuration;
    session.variables_ref = &variable_store;
    session.args_ref = &command.render_args;
    session.ignore_names_ref = &base_variable_context.ignore_names;
    session.effective_settings_cache_ref = &effective_settings_cache;
    session.start_time = start_time;

    RuleResolver rule_resolver;
    BuiltinRegistry builtins;
    ExpressionEvaluator expression_engine(builtins);
    IncludeResolver include_resolver;
    Renderer renderer(rule_resolver, include_resolver, expression_engine);

    RenderReport report;
    report.output_encoding = effective_settings.output_encoding;
    if (shared_template.from_compiled_program) {
        report.output = renderer.render_program(shared_template.compiled_program, effective_settings,
                                                shared_template.compiled_program.logical_path, session);
    } else {
        report.output = renderer.render_source(shared_template.buffer.view(), effective_settings,
                                               command.input_path.value_or(std::filesystem::path{}), session);
    }

    const auto elapsed = std::chrono::steady_clock::now() - start_time;
    report.elapsed_micros = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    report.lua_cache_hits = session.lua_cache_hits;
    report.lua_cache_misses = session.lua_cache_misses;
    return report;
}

std::string format_benchmark(const RenderReport& report) {
    std::ostringstream stream;
    stream << "\n[benchmark] " << report.elapsed_micros << "us"
           << " lua_cache_hits=" << report.lua_cache_hits
           << " lua_cache_misses=" << report.lua_cache_misses;
    return stream.str();
}

}

void BatchProcessor::run(const Command& command) const {
    const std::string output = execute(command);
    if (!output.empty()) {
        OutputWriter writer;
        writer.write(output, std::nullopt);
    }
}

std::string BatchProcessor::execute(const Command& command) const {
    if (command.batch_from_stdin && !command.input_path.has_value()) {
        throw DiagnosticError(make_batch_error("Batch input from stdin requires a template file argument"));
    }
    if (!command.batch_path.has_value() && !command.batch_from_stdin) {
        throw DiagnosticError(make_batch_error("Batch mode requires -b/--batch"));
    }

    JsonParser json_parser;
    const std::string batch_json = command.batch_path.has_value()
        ? read_file(*command.batch_path)
        : read_stdin();
    const Data batch_root = json_parser.parse_string(batch_json);
    std::vector<BatchEntry> entries = parse_batch_entries(batch_root);

    const SettingsData settings = [&command]() {
        if (!command.settings_path.has_value()) {
            return SettingsData{};
        }
        SettingsLoader loader;
        return loader.load(*command.settings_path);
    }();

    ProfileMerger profile_merger;
    RuleResolver rule_resolver;
    const ResolvedConfiguration configuration = rule_resolver.resolve(profile_merger.merge(settings, command.profile_names),
                                                                      command.rule_args, command.ignore_names,
                                                                      command.include_paths, command.debug);
    const EffectiveSettings effective_settings = rule_resolver.resolve_for_file(configuration, command.input_path.value_or(std::filesystem::path{}));
    VariableDefinitionParser variable_parser;
    const VariableContext base_variable_context = variable_parser.parse(command.define_args, configuration.variables,
                                                                        configuration.ignore_names);
    const SharedTemplateInput shared_template = load_template_input(command, effective_settings);

    const bool write_to_directory = command.output_path.has_value() && is_output_directory(*command.output_path);
    if (command.output_path.has_value() && !write_to_directory && entries.size() > 1) {
        throw DiagnosticError(make_batch_error("Batch mode with multiple entries requires an output directory"));
    }

    OutputWriter writer;
    std::ostringstream combined_output;
    const std::filesystem::path template_path = command.input_path.value_or(std::filesystem::path{});

    for (BatchEntry& entry : entries) {
        const std::optional<std::string> output_override = extract_output_override(entry.variables);
        const RenderReport report = render_with_batch_variables(command, shared_template, entry.variables, configuration,
                                                                effective_settings, base_variable_context);
        const std::string output = command.benchmark ? report.output + format_benchmark(report) : report.output;

        if (write_to_directory) {
            std::filesystem::create_directories(*command.output_path);
            const std::filesystem::path output_file = *command.output_path
                / resolve_output_filename(entry, output_override, template_path);
            writer.write(output, output_file, report.output_encoding);
            continue;
        }

        if (command.output_path.has_value()) {
            writer.write(output, command.output_path, report.output_encoding);
            continue;
        }

        combined_output << output;
    }

    if (!write_to_directory && !command.output_path.has_value()) {
        return combined_output.str();
    }

    return {};
}

}
