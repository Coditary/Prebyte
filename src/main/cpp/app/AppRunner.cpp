#include "app/AppRunner.h"

#include "config/ProfileMerger.h"
#include "config/RuleResolver.h"
#include "config/SettingsLoader.h"
#include "config/VariableDefinitionParser.h"
#include "io/InputReader.h"
#include "io/OutputWriter.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/IncludeResolver.h"
#include "runtime/LuaHelperRegistry.h"
#include "runtime/Renderer.h"
#include "support/TextUtil.h"
#include "support/Version.h"

#include <chrono>
#include <sstream>

namespace prebyte {

namespace {

SettingsData load_settings_for_command(const Command& command) {
    if (!command.settings_path.has_value()) {
        return SettingsData{};
    }
    SettingsLoader loader;
    return loader.load(*command.settings_path);
}

ResolvedConfiguration resolve_configuration(const Command& command, const SettingsData& settings) {
    ProfileMerger profile_merger;
    RuleResolver rule_resolver;
    SettingsData merged = profile_merger.merge(settings, command.profile_names);
    return rule_resolver.resolve(merged, command.rule_args, command.ignore_names, command.debug);
}

VariableContext resolve_variables(const Command& command, const ResolvedConfiguration& configuration) {
    VariableDefinitionParser parser;
    return parser.parse(command.define_args, configuration.variables, configuration.ignore_names);
}

std::string format_benchmark(const RenderReport& report) {
    std::ostringstream stream;
    stream << "\n[benchmark] " << report.elapsed_micros << "us"
           << " lua_cache_hits=" << report.lua_cache_hits
           << " lua_cache_misses=" << report.lua_cache_misses;
    return stream.str();
}

std::string format_lua_helper_signatures() {
    LuaHelperRegistry helper_registry;
    std::ostringstream stream;
    bool first = true;
    for (const LuaHelperDefinition& helper : helper_registry.definitions()) {
        if (!first) {
            stream << ", ";
        }
        first = false;
        stream << helper.signature;
    }
    return stream.str();
}

template <typename Range>
std::string format_name_list(const Range& names) {
    std::ostringstream stream;
    for (const auto& name : names) {
        stream << name << '\n';
    }
    return stream.str();
}

}

void AppRunner::run(const Command& command) const {
    OutputWriter writer;
    writer.write(execute(command), command.output_path);
}

std::string AppRunner::execute(const Command& command) const {
    switch (command.mode) {
    case CommandMode::Render: {
        const RenderReport report = render_report(command);
        if (command.benchmark) {
            return report.output + format_benchmark(report);
        }
        return report.output;
    }
    case CommandMode::ListRules:
        return list_rules(command);
    case CommandMode::ListVars:
        return list_vars(command);
    case CommandMode::ListProfiles:
        return list_profiles(command);
    case CommandMode::ListIgnores:
        return list_ignores(command);
    case CommandMode::Explain:
        return explain(command);
    case CommandMode::Help:
        return help();
    case CommandMode::Version:
        return version();
    }

    return {};
}

RenderReport AppRunner::render_report(const Command& command) const {
    const auto start_time = std::chrono::steady_clock::now();
    const SettingsData settings = load_settings_for_command(command);
    RuleResolver rule_resolver;
    const ResolvedConfiguration configuration = resolve_configuration(command, settings);
    const EffectiveSettings effective_settings = rule_resolver.resolve_for_file(configuration, command.input_path.value_or(std::filesystem::path{}));
    const VariableContext variable_context = resolve_variables(command, configuration);

    InputReader reader;
    const InputBuffer input = command.inline_input.has_value()
        ? InputBuffer::from_owned(*command.inline_input)
        : reader.read(command.input_path);

    RenderSession session;
    session.configuration = configuration;
    session.args = command.render_args;
    session.ignore_names = variable_context.ignore_names;
    session.variables.set_all(variable_context.variables);
    session.start_time = start_time;

    BuiltinRegistry builtins;
    ExpressionEvaluator expression_engine(builtins);
    IncludeResolver include_resolver;
    Renderer renderer(rule_resolver, include_resolver, expression_engine);
    RenderReport report;
    report.output = renderer.render_source(input.view(), effective_settings, command.input_path.value_or(std::filesystem::path{}), session);

    const auto elapsed = std::chrono::steady_clock::now() - start_time;
    report.elapsed_micros = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    report.lua_cache_hits = session.lua_cache_hits;
    report.lua_cache_misses = session.lua_cache_misses;
    return report;
}

std::string AppRunner::list_rules(const Command& command) const {
    const SettingsData settings = load_settings_for_command(command);
    const ResolvedConfiguration configuration = resolve_configuration(command, settings);

    std::ostringstream stream;
    stream << "global rules\n";
    for (const auto& [name, value] : configuration.global_rules) {
        stream << name << '=' << value << '\n';
    }
    stream << "file rules\n";
    for (const FileRule& rule : configuration.file_rules) {
        stream << (rule.kind == RuleMatchKind::Extension ? "extension:" : "exact:")
               << rule.pattern << "::" << rule.name << '=' << rule.value << '\n';
    }
    return stream.str();
}

std::string AppRunner::list_vars(const Command& command) const {
    const SettingsData settings = load_settings_for_command(command);
    const ResolvedConfiguration configuration = resolve_configuration(command, settings);
    const VariableContext variables = resolve_variables(command, configuration);

    std::ostringstream stream;
    for (const auto& [name, value] : variables.variables) {
        stream << name << '=' << value << '\n';
    }
    return stream.str();
}

std::string AppRunner::list_profiles(const Command& command) const {
    const SettingsData settings = load_settings_for_command(command);
    ProfileMerger profile_merger;
    static_cast<void>(profile_merger.merge(settings, command.profile_names));

    std::ostringstream stream;
    for (const auto& [name, profile] : settings.profiles) {
        static_cast<void>(profile);
        stream << name << '\n';
    }
    return stream.str();
}

std::string AppRunner::list_ignores(const Command& command) const {
    const SettingsData settings = load_settings_for_command(command);
    const ResolvedConfiguration configuration = resolve_configuration(command, settings);
    return format_name_list(configuration.ignore_names);
}

std::string AppRunner::explain(const Command& command) const {
    const std::string topic = text::to_lower(command.explain_topic.value_or(""));
    if (topic == "rule" || topic == "rules") {
        return "Rules: --rule name=value sets global rule. --rule .ext::name=value scopes by extension. --rule file::name=value scopes exact file. List effective rules with 'list rules'. Lua rules: lua_instruction_limit, lua_memory_limit_bytes.\n";
    }
    if (topic == "ignore" || topic == "ignores") {
        return "Ignore names suppress matching lookups during render. Sources merge from settings ignore, selected profiles, and --ignore/-i. Inspect effective names with 'list ignore' or 'list ignores'.\n";
    }
    if (topic == "profile" || topic == "profiles") {
        return "Profiles are named settings bundles loaded from settings files. Each profile may add variables, rules, file_rules, and ignore entries. Apply with --profile/-p. Inspect available names with 'list profiles'.\n";
    }
    if (topic == "truthiness") {
        return "Native truthiness trims strings and compares case-insensitively. Falsey values: empty string, false, 0, no, off. Numbers are false only when zero; all other non-empty strings are true.\n";
    }
    if (topic == "lua") {
        std::ostringstream stream;
        stream << "Lua helpers: " << format_lua_helper_signatures() << ". "
               << "Sandbox blocks os, io, debug, package, require, dofile, loadfile. "
               << "Limits: lua_instruction_limit=" << EffectiveSettings{}.lua_instruction_limit
               << ", lua_memory_limit_bytes=" << EffectiveSettings{}.lua_memory_limit_bytes << ".\n";
        return stream.str();
    }
    if (topic == "args") {
        return "Render arguments are extra positional values exposed as ARGS[index]. First extra value is ARGS[0]. Use 'prebyte template.txt foo bar' for file input or 'prebyte -- foo bar' for stdin input. Bare ARGS is not supported.\n";
    }
    return "Known topics: rule, ignore, profile, truthiness, lua, ARGS\n";
}

std::string AppRunner::help() const {
    std::ostringstream stream;
    stream << "prebyte [input] [options] [args...]\n"
            "  -o, --output <file>\n"
            "  -Dname=value\n"
            "  -r, --rule <rule>\n"
            "  -s, --settings <file>\n"
            "  -i, --ignore <name>\n"
            "  -p, --profile <name>\n"
            "  --benchmark\n"
            "  -X, --debug\n"
            "  -- [args...] pass render args for stdin mode\n"
            "  list rules|vars|profiles|ignore|ignores [options]\n"
            "Render args: extra positional values after input, or after -- for stdin, available as ARGS[index].\n"
            "Lua helpers: " << format_lua_helper_signatures() << "\n"
            "Lua limits: lua_instruction_limit=" << EffectiveSettings{}.lua_instruction_limit
            << ", lua_memory_limit_bytes=" << EffectiveSettings{}.lua_memory_limit_bytes << "\n";
    return stream.str();
}

std::string AppRunner::version() const {
    return std::string(VERSION) + "\n";
}

}
