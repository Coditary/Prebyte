#include "PrebyteEngine.h"

#include "io/OutputWriter.h"

namespace prebyte {

Prebyte::Prebyte(std::string settings_file) {
    if (!settings_file.empty()) {
        settings_path_ = std::move(settings_file);
    }
}

void Prebyte::set_variable(const std::string& name, const std::string& value) {
    define_args_.push_back(name + '=' + value);
}

void Prebyte::set_variable(const std::string& name) {
    define_args_.push_back(name + '=');
}

void Prebyte::add_argument(const std::string& value) {
    render_args_.push_back(value);
}

void Prebyte::set_profile(const std::string& profile_name) {
    profile_names_.push_back(profile_name);
}

void Prebyte::set_ignore(const std::string& ignore_item) {
    ignore_names_.push_back(ignore_item);
}

void Prebyte::set_rule(const std::string& rule_name, const std::string& rule_value) {
    rule_args_.push_back(rule_name + '=' + rule_value);
}

std::string Prebyte::process(const std::string& input) const {
    AppRunner runner;
    Command command = make_command();
    command.inline_input = input;
    return runner.execute(command);
}

std::string Prebyte::process_file(const std::string& file_path) const {
    AppRunner runner;
    Command command = make_command();
    command.input_path = file_path;
    return runner.execute(command);
}

void Prebyte::process(const std::string& input, const std::string& output_path) const {
    OutputWriter writer;
    writer.write(process(input), output_path);
}

void Prebyte::process_file(const std::string& file_path, const std::string& output_path) const {
    OutputWriter writer;
    writer.write(process_file(file_path), output_path);
}

Command Prebyte::make_command() const {
    Command command;
    command.mode = CommandMode::Render;
    command.settings_path = settings_path_;
    command.render_args = render_args_;
    command.define_args = define_args_;
    command.profile_names = profile_names_;
    command.ignore_names = ignore_names_;
    command.rule_args = rule_args_;
    return command;
}

}
