#include "TestHarness.h"

#include "app/AppRunner.h"
#include "app/Command.h"

#include <filesystem>
#include <fstream>

namespace {

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path structured_import_root(const std::string& name) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "prebyte-structured-import-e2e" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

std::string render_with_import(const std::string& template_source, const std::vector<std::string>& define_args) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = template_source;
    command.define_args = define_args;

    prebyte::AppRunner runner;
    return runner.execute(command);
}

}

TEST_CASE(StructuredImportE2E_json_named_import_renders_member_and_index_access) {
    const std::filesystem::path root = structured_import_root("json");
    const std::filesystem::path data_path = root / "user.json";
    write_file(data_path, R"({"name":"Ada","items":["A","B"]})");

    const std::string output = render_with_import("{{ data.name }}|{{ data.items[1] }}",
                                                  {"data=@" + data_path.string()});
    REQUIRE_EQ(output, std::string("Ada|B"));
}

TEST_CASE(StructuredImportE2E_yaml_named_import_renders_mapping_and_list_access) {
    const std::filesystem::path root = structured_import_root("yaml");
    const std::filesystem::path data_path = root / "catalog.yaml";
    write_file(data_path, "name: Ada\nitems:\n  - A\n  - B\n");

    const std::string output = render_with_import("{{ data.name }}|{{ data.items[1] }}",
                                                  {"data=@" + data_path.string()});
    REQUIRE_EQ(output, std::string("Ada|B"));
}

TEST_CASE(StructuredImportE2E_toml_named_import_renders_nested_table_access) {
    const std::filesystem::path root = structured_import_root("toml");
    const std::filesystem::path data_path = root / "config.toml";
    write_file(data_path, "[server]\nhost=\"localhost\"\nport=8080\n");

    const std::string output = render_with_import("{{ data.server.host }}:{{ data.server.port }}",
                                                  {"data=@" + data_path.string()});
    REQUIRE_EQ(output, std::string("localhost:8080"));
}

TEST_CASE(StructuredImportE2E_ini_named_import_renders_section_member_access) {
    const std::filesystem::path root = structured_import_root("ini");
    const std::filesystem::path data_path = root / "config.ini";
    write_file(data_path, "[server]\nhost = localhost\nport = 8080\n");

    const std::string output = render_with_import("{{ data.server.host }}:{{ data.server.port }}",
                                                  {"data=@" + data_path.string()});
    REQUIRE_EQ(output, std::string("localhost:8080"));
}

TEST_CASE(StructuredImportE2E_env_named_import_renders_key_access) {
    const std::filesystem::path root = structured_import_root("env");
    const std::filesystem::path data_path = root / "app.env";
    write_file(data_path, "NAME=Ada\nROLE=admin\n");

    const std::string output =
        render_with_import("{{ data.NAME }}:{{ data.ROLE }}", {"data=@" + data_path.string()});
    REQUIRE_EQ(output, std::string("Ada:admin"));
}

TEST_CASE(StructuredImportE2E_yaml_list_root_import_renders_index_access) {
    const std::filesystem::path root = structured_import_root("yaml-list-root");
    const std::filesystem::path data_path = root / "items.yaml";
    write_file(data_path, "- Ada\n- Grace\n");

    const std::string output = render_with_import("{{ items[0] }}|{{ items[1] }}", {"items=@" + data_path.string()});
    REQUIRE_EQ(output, std::string("Ada|Grace"));
}

TEST_CASE(StructuredImportE2E_lua_reads_named_structured_import) {
    const std::filesystem::path root = structured_import_root("lua-json");
    const std::filesystem::path data_path = root / "user.json";
    write_file(data_path, R"({"name":"Ada"})");

    const std::string output = render_with_import(
        "{{ data.name }} {{ lua \"return data.name\" }}", {"data=@" + data_path.string()});
    REQUIRE_EQ(output, std::string("Ada Ada"));
}
