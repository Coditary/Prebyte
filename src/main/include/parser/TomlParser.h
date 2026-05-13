#pragma once

#include <fstream>
#include <string>

#include "parser/Parser.h"

namespace prebyte {

class TomlParser : public Parser {
public:
    TomlParser() = default;
    ~TomlParser() override = default;

    Data parse(const std::filesystem::path& filepath) override;
    bool can_parse(const std::filesystem::path& filepath) const override;
    Data parse_string(const std::string& toml_string) override;
};

}
