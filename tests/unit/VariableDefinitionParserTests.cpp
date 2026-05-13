#include "TestHarness.h"

#include "config/VariableDefinitionParser.h"

TEST_CASE(VariableDefinitionParser_inline_and_escaped_at) {
    prebyte::VariableDefinitionParser parser;
    const prebyte::VariableContext context = parser.parse({"name=Ada", "literal=@@keep"}, {}, {});

    REQUIRE_EQ(context.variables.at("name"), std::string("Ada"));
    REQUIRE_EQ(context.variables.at("literal"), std::string("@keep"));
}

TEST_CASE(VariableDefinitionParser_import_env_file) {
    prebyte::VariableDefinitionParser parser;
    const prebyte::VariableContext context = parser.parse({"tests/fixtures/variable_import/sample.env"}, {}, {});

    REQUIRE_EQ(context.variables.at("NAME"), std::string("Ada"));
    REQUIRE_EQ(context.variables.at("CITY"), std::string("Berlin"));
}
