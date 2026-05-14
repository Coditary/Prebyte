#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "config/ConfigTypes.h"

namespace prebyte {

class VariableDefinitionParser {
public:
    VariableContext parse(const std::vector<std::string>& define_args, const std::map<std::string, std::string>& base_variables,
                          const std::set<std::string>& base_ignore_names) const;

private:
    void parse_define(const std::string& define_arg, VariableContext& context) const;
    void import_file(const std::filesystem::path& path, VariableContext& context) const;
    void import_named_file(const std::string& name, const std::filesystem::path& path, VariableContext& context) const;
    void flatten_data(const std::string& prefix, const class Data& data, VariableContext& context) const;
    bool is_structured_import_path(const std::filesystem::path& path) const;
};

}
