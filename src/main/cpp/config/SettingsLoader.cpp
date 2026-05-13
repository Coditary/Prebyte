#include "config/SettingsLoader.h"

#include "datatypes/Data.h"
#include "parser/FileParser.h"
#include "support/Diagnostic.h"

namespace prebyte {

namespace {

Diagnostic make_config_error(const std::string& message, const std::filesystem::path& path = {}) {
    Diagnostic diagnostic;
    diagnostic.code = "CFG001";
    diagnostic.message = message;
    diagnostic.span.file_path = path.string();
    return diagnostic;
}

std::string data_to_string(const Data& value) {
    if (value.is_null()) {
        return "";
    }
    return value.as_string();
}

std::map<std::string, std::string> to_string_map(const Data& value) {
    std::map<std::string, std::string> output;
    if (value.is_null()) {
        return output;
    }
    if (!value.is_map()) {
        throw DiagnosticError(make_config_error("Expected map value in settings"));
    }
    for (const auto& [key, item] : value.as_map()) {
        output[key] = data_to_string(item);
    }
    return output;
}

std::vector<std::string> to_string_list(const Data& value) {
    std::vector<std::string> output;
    if (value.is_null()) {
        return output;
    }
    if (!value.is_array()) {
        throw DiagnosticError(make_config_error("Expected list value in settings"));
    }
    for (const auto& item : value.as_array()) {
        output.push_back(data_to_string(item));
    }
    return output;
}

std::vector<FileRule> parse_file_rules(const Data& value) {
    std::vector<FileRule> file_rules;
    if (value.is_null()) {
        return file_rules;
    }
    if (!value.is_map()) {
        throw DiagnosticError(make_config_error("Expected file_rules to be a map"));
    }

    for (const auto& [pattern, rule_set] : value.as_map()) {
        if (!rule_set.is_map()) {
            throw DiagnosticError(make_config_error("Each file rule entry must be a map"));
        }

        const RuleMatchKind kind = !pattern.empty() && pattern[0] == '.' ? RuleMatchKind::Extension : RuleMatchKind::Exact;
        for (const auto& [rule_name, rule_value] : rule_set.as_map()) {
            file_rules.push_back(FileRule{kind, pattern, rule_name, data_to_string(rule_value)});
        }
    }

    return file_rules;
}

ProfileConfig parse_profile(const Data& value) {
    if (!value.is_map()) {
        throw DiagnosticError(make_config_error("Profile value must be a map"));
    }

    ProfileConfig profile;
    const auto& map = value.as_map();
    if (auto it = map.find("variables"); it != map.end()) {
        profile.variables = to_string_map(it->second);
    }
    if (auto it = map.find("ignore"); it != map.end()) {
        profile.ignore_names = to_string_list(it->second);
    }
    if (auto it = map.find("rules"); it != map.end()) {
        profile.rules = to_string_map(it->second);
    }
    if (auto it = map.find("file_rules"); it != map.end()) {
        profile.file_rules = parse_file_rules(it->second);
    }
    return profile;
}

}

SettingsData SettingsLoader::load(const std::filesystem::path& path) const {
    FileParser file_parser;
    Data data;

    try {
        data = file_parser.parse(path.string());
    } catch (const std::exception& error) {
        throw DiagnosticError(make_config_error(error.what(), path));
    }

    if (!data.is_map()) {
        throw DiagnosticError(make_config_error("Settings root must be a map", path));
    }

    SettingsData settings;
    const auto& map = data.as_map();

    if (auto it = map.find("variables"); it != map.end()) {
        settings.variables = to_string_map(it->second);
    }
    if (auto it = map.find("ignore"); it != map.end()) {
        settings.ignore_names = to_string_list(it->second);
    }
    if (auto it = map.find("rules"); it != map.end()) {
        settings.rules = to_string_map(it->second);
    }
    if (auto it = map.find("file_rules"); it != map.end()) {
        settings.file_rules = parse_file_rules(it->second);
    }
    if (auto it = map.find("profiles"); it != map.end()) {
        if (!it->second.is_map()) {
            throw DiagnosticError(make_config_error("profiles must be a map", path));
        }
        for (const auto& [name, profile_data] : it->second.as_map()) {
            settings.profiles[name] = parse_profile(profile_data);
        }
    }

    return settings;
}

}
