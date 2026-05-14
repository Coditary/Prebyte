#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "config/ConfigTypes.h"

namespace prebyte {

class RuleResolver {
public:
    ResolvedConfiguration resolve(const SettingsData& settings, const std::vector<std::string>& cli_rule_args,
                                  const std::vector<std::string>& cli_ignore_names,
                                  const std::vector<std::filesystem::path>& cli_include_paths,
                                  bool debug_enabled) const;

    EffectiveSettings resolve_for_file(const ResolvedConfiguration& configuration,
                                       const std::filesystem::path& file_path) const;

private:
    std::pair<std::string, std::string> parse_rule_assignment(const std::string& assignment) const;
    FileRule parse_file_rule(const std::string& rule) const;
    void apply_rule(EffectiveSettings& settings, const std::string& name, const std::string& value) const;
};

}
