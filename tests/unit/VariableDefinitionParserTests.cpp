#include "TestHarness.h"

#include "config/VariableDefinitionParser.h"

#include "runtime/FileMetadataCache.h"

#include <filesystem>
#include <fstream>
#include <thread>

namespace {

std::filesystem::path variable_import_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-variable-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void write_variable_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

}

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

TEST_CASE(VariableDefinitionParser_named_structured_json_import) {
    const std::filesystem::path root = variable_import_test_root("named-json");
    const std::filesystem::path path = root / "user.json";
    write_variable_file(path, "{\"name\":\"Ada\",\"active\":true}");

    prebyte::VariableDefinitionParser parser;
    const prebyte::VariableContext context = parser.parse({"user=@" + path.string()}, {}, {});

    REQUIRE(context.structured_variables.contains("user"));
    REQUIRE_EQ(context.variables.contains("user"), false);
    const auto name = context.structured_variables.at("user").member("name");
    REQUIRE(name.has_value());
    REQUIRE_EQ(name->to_string(), std::string("Ada"));
}

TEST_CASE(VariableDefinitionParser_named_structured_yaml_import) {
    const std::filesystem::path root = variable_import_test_root("named-yaml");
    const std::filesystem::path path = root / "items.yaml";
    write_variable_file(path, "- Ada\n- Grace\n");

    prebyte::VariableDefinitionParser parser;
    const prebyte::VariableContext context = parser.parse({"items=@" + path.string()}, {}, {});

    REQUIRE(context.structured_variables.contains("items"));
    REQUIRE(context.structured_variables.at("items").is_list());
    const auto first = context.structured_variables.at("items").index(0);
    REQUIRE(first.has_value());
    REQUIRE_EQ(first->to_string(), std::string("Ada"));
}

TEST_CASE(VariableDefinitionParser_named_structured_toml_import) {
    const std::filesystem::path root = variable_import_test_root("named-toml");
    const std::filesystem::path path = root / "config.toml";
    write_variable_file(path, "[server]\nhost=\"localhost\"\nport=8080\n");

    prebyte::VariableDefinitionParser parser;
    const prebyte::VariableContext context = parser.parse({"config=@" + path.string()}, {}, {});

    REQUIRE(context.structured_variables.contains("config"));
    const auto server = context.structured_variables.at("config").member("server");
    REQUIRE(server.has_value());
    const auto host = server->member("host");
    REQUIRE(host.has_value());
    REQUIRE_EQ(host->to_string(), std::string("localhost"));
}

TEST_CASE(VariableDefinitionParser_named_raw_text_import_for_unknown_extension) {
    const std::filesystem::path root = variable_import_test_root("named-raw");
    const std::filesystem::path path = root / "body.txt";
    write_variable_file(path, "Hello Ada");

    prebyte::VariableDefinitionParser parser;
    const prebyte::VariableContext context = parser.parse({"body=@" + path.string()}, {}, {});

    REQUIRE_EQ(context.variables.at("body"), std::string("Hello Ada"));
    REQUIRE_EQ(context.structured_variables.contains("body"), false);
}

TEST_CASE(VariableDefinitionParser_structured_import_cache_refreshes_on_file_change) {
    const std::filesystem::path root = variable_import_test_root("structured-cache-refresh");
    const std::filesystem::path path = root / "user.json";
    write_variable_file(path, "{\"name\":\"Ada\"}");

    prebyte::VariableDefinitionParser parser;
    const prebyte::VariableContext first = parser.parse({"user=@" + path.string()}, {}, {});
    const auto first_name = first.structured_variables.at("user").member("name");
    REQUIRE(first_name.has_value());
    REQUIRE_EQ(first_name->to_string(), std::string("Ada"));

    std::this_thread::sleep_for(prebyte::FileMetadataCache::ttl());
    write_variable_file(path, "{\"name\":\"Grace\"}");
    prebyte::FileMetadataCache::instance().invalidate(path);

    const prebyte::VariableContext second = parser.parse({"user=@" + path.string()}, {}, {});
    const auto second_name = second.structured_variables.at("user").member("name");
    REQUIRE(second_name.has_value());
    REQUIRE_EQ(second_name->to_string(), std::string("Grace"));
}
