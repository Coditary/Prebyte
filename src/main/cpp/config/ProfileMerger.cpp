#include "config/ProfileMerger.h"

#include "support/Diagnostic.h"

namespace prebyte {

SettingsData ProfileMerger::merge(const SettingsData& settings, const std::vector<std::string>& profile_names) const {
    SettingsData merged = settings;

    for (const std::string& profile_name : profile_names) {
        auto it = settings.profiles.find(profile_name);
        if (it == settings.profiles.end()) {
            Diagnostic diagnostic;
            diagnostic.code = "CFG002";
            diagnostic.message = "Unknown profile: " + profile_name;
            throw DiagnosticError(diagnostic);
        }

        const ProfileConfig& profile = it->second;
        for (const auto& [name, value] : profile.variables) {
            merged.variables[name] = value;
        }
        merged.include_paths.insert(merged.include_paths.end(), profile.include_paths.begin(), profile.include_paths.end());
        merged.ignore_names.insert(merged.ignore_names.end(), profile.ignore_names.begin(), profile.ignore_names.end());
        for (const auto& [name, value] : profile.rules) {
            merged.rules[name] = value;
        }
        merged.file_rules.insert(merged.file_rules.end(), profile.file_rules.begin(), profile.file_rules.end());
    }

    return merged;
}

}
