#include "TestHarness.h"

#include "parser/TomlParser.h"

#include <filesystem>
#include <fstream>

namespace {

void write_parser_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path parser_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-toml-parser-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

}

TEST_CASE(TomlParser_reject_deeply_nested_table_path) {
    std::string toml = "[";
    for (int index = 0; index < 200; ++index) {
        if (index != 0) {
            toml.push_back('.');
        }
        toml += "section";
    }
    toml += "]\nvalue = 1";

    prebyte::TomlParser parser;
    REQUIRE_THROWS_AS(parser.parse_string(toml), std::runtime_error);
}

TEST_CASE(TomlParser_parse_string_sections_values_and_arrays) {
    prebyte::TomlParser parser;
    const prebyte::Data data = parser.parse_string(R"(
name = Ada
enabled = true
count = 2
ratio = 3.5
tags = [ "a", "b" ]
[server]
host = "localhost"
port = 8080
[database.credentials]
user = "ada"
)");

    REQUIRE_EQ(data.as_map().at("name").as_string(), std::string("Ada"));
    REQUIRE(data.as_map().at("enabled").as_bool());
    REQUIRE_EQ(data.as_map().at("count").as_int(), 2);
    REQUIRE_EQ(data.as_map().at("tags").as_array().size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(data.as_map().at("server").as_map().at("host").as_string(), std::string("localhost"));
    REQUIRE_EQ(data.as_map().at("database").as_map().at("credentials").as_map().at("user").as_string(),
                 std::string("ada"));
}

TEST_CASE(TomlParser_can_parse_valid_and_reject_invalid_files) {
    const std::filesystem::path root = parser_test_root("toml-file");
    const std::filesystem::path path = root / "settings.toml";
    const std::filesystem::path wrong_ext = root / "settings.txt";
    write_parser_file(path, "name = Ada\n");
    write_parser_file(wrong_ext, "name = Ada\n");

    prebyte::TomlParser parser;
    REQUIRE(parser.can_parse(path));
    REQUIRE(!parser.can_parse(wrong_ext));
    REQUIRE_THROWS_AS(parser.parse_string("broken line"), std::runtime_error);
}
