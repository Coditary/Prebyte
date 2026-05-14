#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace prebyte {

enum class CommandMode {
    Render,
    ListRules,
    ListVars,
    ListProfiles,
    ListIgnores,
    Explain,
    Help,
    Version,
};

struct Command {
    CommandMode mode = CommandMode::Render;
    std::optional<std::filesystem::path> input_path;
    std::optional<std::filesystem::path> output_path;
    std::optional<std::string> inline_input;
    std::vector<std::string> render_args;
    std::vector<std::filesystem::path> include_paths;
    std::vector<std::string> define_args;
    std::vector<std::string> rule_args;
    std::vector<std::string> ignore_names;
    std::vector<std::string> profile_names;
    std::optional<std::filesystem::path> settings_path;
    std::optional<std::string> explain_topic;
    bool benchmark = false;
    bool debug = false;
};

}
