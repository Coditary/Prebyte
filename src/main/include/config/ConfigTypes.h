#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace prebyte {

enum class RuleMatchKind {
    Extension,
    Exact,
};

struct FileRule {
    RuleMatchKind kind = RuleMatchKind::Exact;
    std::string pattern;
    std::string name;
    std::string value;
};

struct ProfileConfig {
    std::map<std::string, std::string> variables;
    std::vector<std::string> ignore_names;
    std::map<std::string, std::string> rules;
    std::vector<FileRule> file_rules;
};

struct SettingsData {
    std::map<std::string, std::string> variables;
    std::vector<std::string> ignore_names;
    std::map<std::string, std::string> rules;
    std::vector<FileRule> file_rules;
    std::map<std::string, ProfileConfig> profiles;
};

struct EffectiveSettings {
    bool strict_variables = false;
    bool case_sensitive_variables = true;
    std::optional<std::string> default_variable_value;
    std::string variable_prefix = "{{";
    std::string variable_suffix = "}}";
    std::size_t max_variable_length = 0;
    bool has_max_variable_length = false;
    bool replace_tabs = false;
    std::size_t tab_size = 4;
    bool trim = false;
    bool allow_includes = true;
    std::filesystem::path include_path;
    std::string output_encoding = "utf-8";
    bool allow_env = false;
    bool error_on_false_input = true;
    std::size_t lua_instruction_limit = 100000;
    std::size_t lua_memory_limit_bytes = 4 * 1024 * 1024;
    bool debug = false;
};

struct ResolvedConfiguration {
    std::map<std::string, std::string> variables;
    std::set<std::string> ignore_names;
    std::map<std::string, std::string> global_rules;
    std::vector<FileRule> file_rules;
    EffectiveSettings base_settings;
};

struct VariableContext {
    std::map<std::string, std::string> variables;
    std::set<std::string> ignore_names;
};

}
