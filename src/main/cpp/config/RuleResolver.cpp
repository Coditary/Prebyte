#include "config/RuleResolver.h"

#include "support/Diagnostic.h"
#include "support/TextUtil.h"

namespace prebyte {

namespace {

Diagnostic make_rule_error(const std::string& message) {
    Diagnostic diagnostic;
    diagnostic.code = "CFG003";
    diagnostic.message = message;
    return diagnostic;
}

bool parse_bool(const std::string& value) {
    if (value == "true" || value == "1") {
        return true;
    }
    if (value == "false" || value == "0") {
        return false;
    }
    throw DiagnosticError(make_rule_error("Expected boolean value, got: " + value));
}

std::size_t parse_size(const std::string& value) {
    try {
        return static_cast<std::size_t>(std::stoull(value));
    } catch (const std::exception&) {
        throw DiagnosticError(make_rule_error("Expected unsigned integer value, got: " + value));
    }
}

std::set<std::string> parse_name_set(const std::string& value) {
    std::set<std::string> names;
    for (std::string entry : text::split(value, ',')) {
        entry = text::trim(std::move(entry));
        if (!entry.empty()) {
            names.insert(std::move(entry));
        }
    }
    return names;
}

std::string parse_output_encoding(const std::string& value) {
    const std::string normalized = text::to_lower(text::trim(value));
    if (normalized == "utf-8" || normalized == "utf-16") {
        return normalized;
    }
    throw DiagnosticError(make_rule_error("Unsupported output encoding: " + value));
}

}

ResolvedConfiguration RuleResolver::resolve(const SettingsData& settings, const std::vector<std::string>& cli_rule_args,
                                            const std::vector<std::string>& cli_ignore_names,
                                            const std::vector<std::filesystem::path>& cli_include_paths,
                                            bool debug_enabled) const {
    ResolvedConfiguration configuration;
    configuration.variables = settings.variables;
    configuration.ignore_names = std::set<std::string>(settings.ignore_names.begin(), settings.ignore_names.end());
    configuration.global_rules = settings.rules;
    configuration.file_rules = settings.file_rules;

    configuration.ignore_names.insert(cli_ignore_names.begin(), cli_ignore_names.end());

    for (const std::string& rule_arg : cli_rule_args) {
        if (rule_arg.find("::") != std::string::npos) {
            configuration.file_rules.push_back(parse_file_rule(rule_arg));
            continue;
        }

        const auto [name, value] = parse_rule_assignment(rule_arg);
        configuration.global_rules[name] = value;
    }

    if (debug_enabled) {
        configuration.global_rules["debug"] = "true";
    }

    for (const auto& [name, value] : configuration.global_rules) {
        apply_rule(configuration.base_settings, name, value);
    }

    configuration.base_settings.include_paths = cli_include_paths;
    configuration.base_settings.include_paths.insert(configuration.base_settings.include_paths.end(),
                                                     settings.include_paths.begin(), settings.include_paths.end());

    return configuration;
}

EffectiveSettings RuleResolver::resolve_for_file(const ResolvedConfiguration& configuration,
                                                 const std::filesystem::path& file_path) const {
    EffectiveSettings settings = configuration.base_settings;
    const std::string file_name = file_path.filename().string();
    const std::string extension = file_path.extension().string();

    for (const FileRule& file_rule : configuration.file_rules) {
        const bool matches = (file_rule.kind == RuleMatchKind::Extension && file_rule.pattern == extension)
            || (file_rule.kind == RuleMatchKind::Exact && file_rule.pattern == file_name);
        if (!matches) {
            continue;
        }
        apply_rule(settings, file_rule.name, file_rule.value);
    }

    return settings;
}

std::pair<std::string, std::string> RuleResolver::parse_rule_assignment(const std::string& assignment) const {
    const std::size_t equals = assignment.find('=');
    if (equals == std::string::npos || equals == 0 || equals + 1 >= assignment.size()) {
        throw DiagnosticError(make_rule_error("Invalid rule assignment: " + assignment));
    }
    return {assignment.substr(0, equals), assignment.substr(equals + 1)};
}

FileRule RuleResolver::parse_file_rule(const std::string& rule) const {
    const std::size_t separator = rule.find("::");
    if (separator == std::string::npos) {
        throw DiagnosticError(make_rule_error("Invalid file rule: " + rule));
    }

    FileRule file_rule;
    file_rule.pattern = rule.substr(0, separator);
    file_rule.kind = !file_rule.pattern.empty() && file_rule.pattern[0] == '.' ? RuleMatchKind::Extension : RuleMatchKind::Exact;

    const auto [name, value] = parse_rule_assignment(rule.substr(separator + 2));
    file_rule.name = name;
    file_rule.value = value;
    return file_rule;
}

void RuleResolver::apply_rule(EffectiveSettings& settings, const std::string& name, const std::string& value) const {
    if (name == "strict_variables") {
        settings.strict_variables = parse_bool(value);
        return;
    }
    if (name == "case_sensitive_variables") {
        settings.case_sensitive_variables = parse_bool(value);
        return;
    }
    if (name == "default_variable_value") {
        settings.default_variable_value = value;
        return;
    }
    if (name == "variable_prefix") {
        settings.variable_prefix = value;
        return;
    }
    if (name == "variable_suffix") {
        settings.variable_suffix = value;
        return;
    }
    if (name == "max_variable_length") {
        settings.max_variable_length = parse_size(value);
        settings.has_max_variable_length = true;
        return;
    }
    if (name == "replace_tabs") {
        settings.replace_tabs = parse_bool(value);
        return;
    }
    if (name == "tab_size") {
        settings.tab_size = parse_size(value);
        return;
    }
    if (name == "trim") {
        settings.trim = parse_bool(value);
        return;
    }
    if (name == "allow_includes") {
        settings.allow_includes = parse_bool(value);
        return;
    }
    if (name == "include_path") {
        settings.include_path = value;
        return;
    }
    if (name == "output_encoding") {
        settings.output_encoding = parse_output_encoding(value);
        return;
    }
    if (name == "allow_env") {
        settings.allow_env = parse_bool(value);
        return;
    }
    if (name == "forbidden_env_vars") {
        settings.forbidden_env_vars = parse_name_set(value);
        return;
    }
    if (name == "error_on_false_input") {
        settings.error_on_false_input = parse_bool(value);
        return;
    }
    if (name == "lua_instruction_limit") {
        settings.lua_instruction_limit = parse_size(value);
        return;
    }
    if (name == "lua_memory_limit_bytes") {
        settings.lua_memory_limit_bytes = parse_size(value);
        return;
    }
    if (name == "max_include_depth") {
        settings.max_include_depth = parse_size(value);
        return;
    }
    if (name == "max_render_time_ms") {
        settings.max_render_time_ms = parse_size(value);
        return;
    }
    if (name == "max_output_size_bytes") {
        settings.max_output_size_bytes = parse_size(value);
        return;
    }
    if (name == "max_loop_iteration") {
        settings.max_loop_iteration = parse_size(value);
        return;
    }
    if (name == "debug") {
        settings.debug = parse_bool(value);
        return;
    }

    throw DiagnosticError(make_rule_error("Unknown rule: " + name));
}

}
