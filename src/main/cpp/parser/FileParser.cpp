#include "parser/FileParser.h"

namespace prebyte {

Data FileParser::parse(const std::string& filePath) {
    Parser* parser;

    if (filePath.empty()) {
        throw std::runtime_error("File path cannot be empty");
    }

    if (!std::filesystem::exists(filePath) || !std::filesystem::is_regular_file(filePath)) {
        throw std::runtime_error("File does not exist or is not a regular file: " + filePath);
    }

    if (filePath.ends_with(".json")) {
        parser = new JsonParser();
    } else if (filePath.ends_with(".yaml") || filePath.ends_with(".yml")) {
        parser = new YamlParser();
    } else if (filePath.ends_with(".ini") || filePath.ends_with(".cfg")) {
        parser = new IniParser();
    } else if (filePath.ends_with(".env")) {
        parser = new EnvParser();
    } else if (filePath.ends_with(".toml")) {
        parser = new TomlParser();
    } else {
        throw std::runtime_error("Unsupported file format: " + filePath);
    }
    return parseFile(filePath, parser);
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

        return Data();
}

}
