#pragma once

#include <fstream>
#include <string>

#include "parser/Parser.h"

namespace prebyte {

class YamlParser : public Parser {
public:
    YamlParser() = default;
    ~YamlParser() override = default;

    Data parse(const std::filesystem::path& filepath) override;
    bool can_parse(const std::filesystem::path& filepath) const override;
    Data parse_string(const std::string& yaml_string) override;
};

}
