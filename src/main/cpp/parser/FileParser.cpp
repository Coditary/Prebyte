#include "parser/FileParser.h"

namespace prebyte {

Data FileParser::parse(const std::string& filePath) {
    if (filePath.empty()) {
        throw std::runtime_error("File path cannot be empty");
    }

    if (!std::filesystem::exists(filePath) || !std::filesystem::is_regular_file(filePath)) {
        throw std::runtime_error("File does not exist or is not a regular file: " + filePath);
    }

    if (filePath.ends_with(".json")) {
        JsonParser parser;
        return parseFile(filePath, &parser);
    }
    if (filePath.ends_with(".yaml") || filePath.ends_with(".yml")) {
        YamlParser parser;
        return parseFile(filePath, &parser);
    }
    if (filePath.ends_with(".ini") || filePath.ends_with(".cfg")) {
        IniParser parser;
        return parseFile(filePath, &parser);
    }
    if (filePath.ends_with(".env")) {
        EnvParser parser;
        return parseFile(filePath, &parser);
    }
    if (filePath.ends_with(".toml")) {
        TomlParser parser;
        return parseFile(filePath, &parser);
    }

    throw std::runtime_error("Unsupported file format: " + filePath);
}

Data FileParser::parseFile(const std::string& filePath, Parser* parser) {
    if (!parser->can_parse(filePath)) {
        throw std::runtime_error("Cannot parse file with the selected parser: " + filePath);
    }

    try {
        return parser->parse(std::filesystem::path(filePath));
    } catch (const std::exception& e) {
        throw std::runtime_error("Error parsing file: " + std::string(e.what()));
    }
}

}
