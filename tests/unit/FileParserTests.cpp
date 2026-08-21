#include "TestHarness.h"

#include "parser/FileParser.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <string_view>

namespace {

void write_parser_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path file_parser_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-file-parser-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void require_runtime_error_with_prefix(const std::function<void()>& action, std::string_view prefix) {
    try {
        action();
        REQUIRE(false);
    } catch (const std::runtime_error& error) {
        REQUIRE(std::string(error.what()).find(std::string(prefix)) == 0);
    }
}

}

TEST_CASE(FileParser_rejects_empty_missing_and_unsupported_paths) {
    prebyte::FileParser parser;
    const std::filesystem::path root = file_parser_test_root("invalid-paths");

    REQUIRE_THROWS_AS(parser.parse(""), std::runtime_error);
    REQUIRE_THROWS_AS(parser.parse((root / "missing.json").string()), std::runtime_error);
    write_parser_file(root / "notes.txt", "plain text");
    REQUIRE_THROWS_AS(parser.parse((root / "notes.txt").string()), std::runtime_error);
}

TEST_CASE(FileParser_parses_supported_extensions) {
    const std::filesystem::path root = file_parser_test_root("supported-extensions");
    write_parser_file(root / "data.json", R"({"name":"Ada","items":[1,2]})");
    write_parser_file(root / "settings.yaml", "name: Ada\n");
    write_parser_file(root / "profile.yml", "name: Grace\n");
    write_parser_file(root / "server.ini", "[server]\nhost = localhost\n");
    write_parser_file(root / "legacy.cfg", "name = Linus\n");
    write_parser_file(root / "sample.env", "NAME=Alan\n");
    write_parser_file(root / "config.toml", "name = \"Katherine\"\n");

    prebyte::FileParser parser;

    REQUIRE_EQ(parser.parse((root / "data.json").string()).as_map().at("name").as_string(), std::string("Ada"));
    REQUIRE_EQ(parser.parse((root / "settings.yaml").string()).as_map().at("name").as_string(), std::string("Ada"));
    REQUIRE_EQ(parser.parse((root / "profile.yml").string()).as_map().at("name").as_string(), std::string("Grace"));
    REQUIRE_EQ(parser.parse((root / "server.ini").string()).as_map().at("server").as_map().at("host").as_string(),
               std::string("localhost"));
    REQUIRE_EQ(parser.parse((root / "legacy.cfg").string()).as_map().at("name").as_string(), std::string("Linus"));
    REQUIRE_EQ(parser.parse((root / "sample.env").string()).as_map().at("NAME").as_string(), std::string("Alan"));
    REQUIRE_EQ(parser.parse((root / "config.toml").string()).as_map().at("name").as_string(), std::string("Katherine"));
}

TEST_CASE(FileParser_rejects_invalid_content_for_all_formats) {
    const std::filesystem::path root = file_parser_test_root("invalid-content");
    write_parser_file(root / "broken.json", R"({"name":"Ada" trailing)");
    write_parser_file(root / "broken.yaml", "missing colon line\n");
    write_parser_file(root / "broken.yml", "missing colon line\n");
    write_parser_file(root / "broken.ini", "; comment only\n");
    write_parser_file(root / "broken.cfg", "plain text without section or equals\n");
    write_parser_file(root / "broken.env", "INVALID LINE WITHOUT EQUALS\n");
    write_parser_file(root / "broken.toml", "broken line without equals\n");

    prebyte::FileParser parser;
    const std::string cannot_parse_prefix = "Cannot parse file with the selected parser:";

    for (const char* filename :
         {"broken.json", "broken.yaml", "broken.yml", "broken.ini", "broken.cfg", "broken.env", "broken.toml"}) {
        require_runtime_error_with_prefix(
            [&]() { (void)parser.parse((root / filename).string()); }, cannot_parse_prefix);
    }
}

TEST_CASE(FileParser_rejects_valid_content_with_wrong_extension) {
    const std::filesystem::path root = file_parser_test_root("wrong-extension");
    write_parser_file(root / "data.txt", R"({"name":"Ada"})");
    write_parser_file(root / "settings.txt", "name: Ada\n");
    write_parser_file(root / "profile.txt", "name: Grace\n");
    write_parser_file(root / "server.txt", "[server]\nhost = localhost\n");
    write_parser_file(root / "legacy.txt", "name = Linus\n");
    write_parser_file(root / "sample.txt", "NAME=Alan\n");
    write_parser_file(root / "config.txt", "name = \"Katherine\"\n");

    prebyte::FileParser parser;
    const std::string unsupported_prefix = "Unsupported file format:";

    for (const char* filename :
         {"data.txt", "settings.txt", "profile.txt", "server.txt", "legacy.txt", "sample.txt", "config.txt"}) {
        require_runtime_error_with_prefix(
            [&]() { (void)parser.parse((root / filename).string()); }, unsupported_prefix);
    }
}

TEST_CASE(FileParser_rejects_additional_invalid_payloads) {
    const std::filesystem::path root = file_parser_test_root("additional-invalid");
    write_parser_file(root / "empty-object-trailing.json", "{} trailing");
    write_parser_file(root / "unclosed-string.json", R"({"name":"x})");
    write_parser_file(root / "deep-toml.toml", [&]() {
        std::string toml = "[";
        for (int index = 0; index < 200; ++index) {
            if (index != 0) {
                toml.push_back('.');
            }
            toml += "section";
        }
        toml += "]\nvalue = 1";
        return toml;
    }());
    write_parser_file(root / "empty.ini", "\n\n");
    write_parser_file(root / "empty.cfg", "# only comments\n");

    prebyte::FileParser parser;
    const std::string cannot_parse_prefix = "Cannot parse file with the selected parser:";

    for (const char* filename :
         {"empty-object-trailing.json", "unclosed-string.json", "deep-toml.toml", "empty.ini", "empty.cfg"}) {
        require_runtime_error_with_prefix(
            [&]() { (void)parser.parse((root / filename).string()); }, cannot_parse_prefix);
    }
}
