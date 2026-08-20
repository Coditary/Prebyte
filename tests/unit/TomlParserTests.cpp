#include "TestHarness.h"

#include "parser/TomlParser.h"

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
