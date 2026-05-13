#pragma once

#include <fstream>
#include <string>

#include "parser/Parser.h"

namespace prebyte {

class JsonParser : public Parser {
public:
    JsonParser() = default;
    ~JsonParser() override = default;

    Data parse(const std::filesystem::path& filepath) override;
    bool can_parse(const std::filesystem::path& filepath) const override;
    Data parse_string(const std::string& json_string) override;
};

}
