#include "config/VariableDefinitionParser.h"

#include "datatypes/Data.h"
#include "parser/FileParser.h"
#include "support/Diagnostic.h"

#include <fstream>
#include <iterator>

namespace prebyte {

namespace {

Diagnostic make_variable_error(const std::string& message) {
    Diagnostic diagnostic;
    diagnostic.code = "CFG004";
    diagnostic.message = message;
    return diagnostic;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw DiagnosticError(make_variable_error("Cannot open file for variable import: " + path.string()));
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

}

VariableContext VariableDefinitionParser::parse(const std::vector<std::string>& define_args,
                                                const std::map<std::string, std::string>& base_variables,
                                                const std::set<std::string>& base_ignore_names) const {
    VariableContext context;
    context.variables = base_variables;
    context.ignore_names = base_ignore_names;

    for (const std::string& define_arg : define_args) {
        parse_define(define_arg, context);
    }

    return context;
}

void VariableDefinitionParser::parse_define(const std::string& define_arg, VariableContext& context) const {
    const std::size_t equals = define_arg.find('=');
    if (equals == std::string::npos) {
        import_file(define_arg, context);
        return;
    }

    if (equals == 0) {
        throw DiagnosticError(make_variable_error("Variable name cannot be empty"));
    }

    const std::string name = define_arg.substr(0, equals);
    std::string value = define_arg.substr(equals + 1);

    if (value.size() >= 2 && value[0] == '@' && value[1] == '@') {
        context.variables[name] = value.substr(1);
        return;
    }

    if (!value.empty() && value[0] == '@') {
        context.variables[name] = read_file(value.substr(1));
        return;
    }

    context.variables[name] = value;
}

void VariableDefinitionParser::import_file(const std::filesystem::path& path, VariableContext& context) const {
    FileParser file_parser;
    Data data;
    try {
        data = file_parser.parse(path.string());
    } catch (const std::exception& error) {
        throw DiagnosticError(make_variable_error(error.what()));
    }
    flatten_data("", data, context);
}

void VariableDefinitionParser::flatten_data(const std::string& prefix, const Data& data, VariableContext& context) const {
    if (data.is_map()) {
        for (const auto& [key, value] : data.as_map()) {
            const std::string full_key = prefix.empty() ? key : prefix + '.' + key;
            flatten_data(full_key, value, context);
        }
        return;
    }

    if (data.is_array()) {
        for (std::size_t index = 0; index < data.as_array().size(); ++index) {
            const std::string full_key = prefix + '.' + std::to_string(index);
            flatten_data(full_key, data[index], context);
        }
        return;
    }

    context.variables[prefix] = data.is_null() ? std::string() : data.as_string();
}

}
