#pragma once

#include <string>
#include <filesystem>

#include "datatypes/Data.h"
#include "parser/Parser.h"
#include "parser/JsonParser.h"
#include "parser/YamlParser.h"
#include "parser/IniParser.h"
#include "parser/EnvParser.h"
#include "parser/TomlParser.h"

namespace prebyte {

/**
 * @brief Handles parsing of files into structured `Data` objects.
 *
 * The `FileParser` class provides functionality to read and interpret
 * files (such as configuration or input files) and convert their contents
 * into `Data` objects. The format and structure of the file is expected
 * to be supported by the underlying `Parser` used internally.
 *
 * This class abstracts away file I/O and parsing logic to simplify usage
 * elsewhere in the application.
 */
class FileParser {
public:
    /** @brief Default constructor. Creates a new file parser instance. */
    FileParser() = default;

    /**
     * @brief Parses a file at the given path into a `Data` object.
     *
     * This is the main entry point for file parsing. It reads the file's content,
     * uses the internal parser to interpret its structure, and returns a corresponding
     * `Data` representation.
     *
     * @param filePath Absolute or relative path to the input file.
     * @return A `Data` object representing the structured file content.
     * @throws std::runtime_error if the file cannot be read or parsed.
     */
    Data parse(const std::string& filePath);

private:
    /**
     * @brief Internal helper to parse the file using a specified parser.
     *
     * This method is used internally to delegate parsing to a specific parser
     * instance, such as a JSON or YAML parser.
     *
     * @param filePath The path to the file being parsed.
     * @param parser Pointer to the parser to use.
     * @return Parsed `Data` structure.
     */
    Data parseFile(const std::string& filePath, Parser* parser);
};

}
