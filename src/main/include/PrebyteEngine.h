#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "app/AppRunner.h"

namespace prebyte {

class Prebyte {
public:
    Prebyte() = default;
    explicit Prebyte(std::string settings_file);

    void set_variable(const std::string& name, const std::string& value);
    void set_variable(const std::string& name);
    void add_argument(const std::string& value);
    void set_profile(const std::string& profile_name);
    void set_ignore(const std::string& ignore_item);
    void set_rule(const std::string& rule_name, const std::string& rule_value);

    std::string process(const std::string& input) const;
    std::string process_file(const std::string& file_path) const;
    void process(const std::string& input, const std::string& output_path) const;
    void process_file(const std::string& file_path, const std::string& output_path) const;

private:
    Command make_command() const;

    std::optional<std::filesystem::path> settings_path_;
    std::vector<std::string> render_args_;
    std::vector<std::string> define_args_;
    std::vector<std::string> profile_names_;
    std::vector<std::string> ignore_names_;
    std::vector<std::string> rule_args_;
};

}
