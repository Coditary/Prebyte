#include "TestHarness.h"

#include "parser/EnvParser.h"
#include "parser/IniParser.h"
#include "parser/JsonParser.h"

#include <filesystem>
#include <fstream>

namespace {

void write_parser_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path parser_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-parser-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

}

TEST_CASE(JsonParser_parse_string_nested_values_and_escapes) {
    prebyte::JsonParser parser;
    const prebyte::Data data = parser.parse_string(R"({
        "name": "Ada",
        "count": 2,
        "ratio": 3.5,
        "active": true,
        "enabled": false,
        "missing": null,
        "empty_obj": {},
        "empty_arr": [],
        "meta": {"line": "a\nb", "quote": "\"", "slash": "\\"},
        "tags": ["x", "y"]
    })");

    const auto& map = data.as_map();
    REQUIRE_EQ(map.at("name").as_string(), std::string("Ada"));
    REQUIRE_EQ(map.at("count").as_int(), 2);
    REQUIRE_EQ(map.at("ratio").as_double(), 3.5);
    REQUIRE(map.at("active").as_bool());
    REQUIRE(!map.at("enabled").as_bool());
    REQUIRE(map.at("missing").is_null());
    REQUIRE(map.at("empty_obj").as_map().empty());
    REQUIRE(map.at("empty_arr").as_array().empty());
    REQUIRE_EQ(map.at("meta").as_map().at("line").as_string(), std::string("a\nb"));
    REQUIRE_EQ(map.at("meta").as_map().at("quote").as_string(), std::string("\""));
    REQUIRE_EQ(map.at("meta").as_map().at("slash").as_string(), std::string("\\"));
    REQUIRE_EQ(map.at("tags").as_array()[1].as_string(), std::string("y"));
}

TEST_CASE(JsonParser_parse_file_and_can_parse_paths) {
    const std::filesystem::path root = parser_test_root("json-file");
    const std::filesystem::path path = root / "config.json";
    write_parser_file(path, "{\"name\":\"Ada\",\"items\":[1,2]}");

    prebyte::JsonParser parser;
    REQUIRE(parser.can_parse(path));
    const prebyte::Data data = parser.parse(path);
    REQUIRE_EQ(data.as_map().at("name").as_string(), std::string("Ada"));
    REQUIRE_EQ(data.as_map().at("items").as_array()[0].as_int(), 1);
}

TEST_CASE(JsonParser_error_paths_and_wrong_extension_fail) {
    const std::filesystem::path root = parser_test_root("json-errors");
    const std::filesystem::path invalid = root / "bad.json";
    const std::filesystem::path wrong_ext = root / "bad.txt";
    write_parser_file(invalid, "{} trailing");
    write_parser_file(wrong_ext, "{}");

    prebyte::JsonParser parser;
    REQUIRE_THROWS_AS(parser.parse_string("{} trailing"), std::runtime_error);
    REQUIRE_THROWS_AS(parser.parse_string(R"({"bad":"\u1234"})"), std::runtime_error);
    REQUIRE_THROWS_AS(parser.parse_string("{\"bad\":\"x}"), std::runtime_error);
    REQUIRE_THROWS_AS(parser.parse_string("-"), std::runtime_error);
    REQUIRE(!parser.can_parse(invalid));
    REQUIRE(!parser.can_parse(wrong_ext));
}

TEST_CASE(IniParser_parse_string_and_file_with_sections) {
    const std::filesystem::path root = parser_test_root("ini-file");
    const std::filesystem::path path = root / "settings.ini";
    const std::string input = "; comment\nname = Ada\n[server]\nhost = localhost\nport = 8080\nignored line\n";
    write_parser_file(path, input);

    prebyte::IniParser parser;
    const prebyte::Data from_string = parser.parse_string(input);
    REQUIRE_EQ(from_string.as_map().at("name").as_string(), std::string("Ada"));
    REQUIRE_EQ(from_string.as_map().at("server").as_map().at("host").as_string(), std::string("localhost"));
    REQUIRE_EQ(from_string.as_map().at("server").as_map().at("port").as_string(), std::string("8080"));

    REQUIRE(parser.can_parse(path));
    const prebyte::Data from_file = parser.parse(path);
    REQUIRE_EQ(from_file.as_map().at("server").as_map().at("host").as_string(), std::string("localhost"));
}

TEST_CASE(IniParser_can_parse_false_for_wrong_extension_or_content) {
    const std::filesystem::path root = parser_test_root("ini-invalid");
    const std::filesystem::path invalid = root / "plain.ini";
    const std::filesystem::path wrong_ext = root / "plain.txt";
    write_parser_file(invalid, "comment only\nnext line\n");
    write_parser_file(wrong_ext, "name = Ada\n");

    prebyte::IniParser parser;
    REQUIRE(!parser.can_parse(invalid));
    REQUIRE(!parser.can_parse(wrong_ext));
}

TEST_CASE(EnvParser_parse_string_and_file_and_can_parse) {
    const std::filesystem::path root = parser_test_root("env-file");
    const std::filesystem::path path = root / "sample.env";
    const std::string input = "# comment\n NAME = Ada \nEMPTY=\n VALUE = spaced value  \n";
    write_parser_file(path, input);

    prebyte::EnvParser parser;
    const prebyte::Data from_string = parser.parse_string(input);
    REQUIRE_EQ(from_string.as_map().at("NAME").as_string(), std::string("Ada"));
    REQUIRE_EQ(from_string.as_map().at("EMPTY").as_string(), std::string());
    REQUIRE_EQ(from_string.as_map().at("VALUE").as_string(), std::string("spaced value"));

    REQUIRE(parser.can_parse(path));
    const prebyte::Data from_file = parser.parse(path);
    REQUIRE_EQ(from_file.as_map().at("NAME").as_string(), std::string("Ada"));
}

TEST_CASE(EnvParser_invalid_lines_and_wrong_extension_fail) {
    const std::filesystem::path root = parser_test_root("env-invalid");
    const std::filesystem::path invalid = root / "bad.env";
    const std::filesystem::path wrong_ext = root / "bad.txt";
    write_parser_file(invalid, "BROKEN\n");
    write_parser_file(wrong_ext, "NAME=Ada\n");

    prebyte::EnvParser parser;
    REQUIRE_THROWS_AS(parser.parse_string("BROKEN"), std::runtime_error);
    REQUIRE(!parser.can_parse(invalid));
    REQUIRE(!parser.can_parse(wrong_ext));
}
