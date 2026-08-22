#include "TestHarness.h"

#include "config/ConfigTypes.h"
#include "parser/TomlParser.h"
#include "runtime/compiled/CompiledTemplateCompiler.h"
#include "runtime/compiled/CompiledTemplateSerializer.h"
#include "runtime/cache/FileMetadataCache.h"
#include "runtime/resolution/IncludeResolver.h"
#include "runtime/lua/LuaRuntime.h"
#include "support/Diagnostic.h"
#include "template/lexer/TemplateLexer.h"
#include "template/parser/TemplateParser.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

void append_u32(std::string& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<char>((value >> shift) & 0xffu));
    }
}

void append_string(std::string& out, std::string_view value) {
    append_u32(out, static_cast<std::uint32_t>(value.size()));
    out.append(value.data(), value.size());
}

std::string make_compiled_header_with_counts(std::uint32_t template_count) {
    std::string bytes;
    bytes.append("PBC1", 4);
    append_u32(bytes, 8);
    append_string(bytes, "logical.pbt");
    append_string(bytes, "source.pbt");
    append_string(bytes, "{{");
    append_string(bytes, "}}");
    append_u32(bytes, 0);
    append_u32(bytes, 4);
    append_u32(bytes, 0);
    append_u32(bytes, template_count);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    return bytes;
}

std::string serialize_minimal_program() {
    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    const prebyte::CompiledProgram program =
        compiler.compile_source("Hello {{ name }}\n", "seed.pbt", "seed.pbt", settings);
    prebyte::CompiledTemplateSerializer serializer;
    return serializer.serialize(program);
}

std::filesystem::path resolver_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-fuzzer-regression" / name;
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root);
    return root;
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::string nested_toml_array(int depth) {
    std::string value;
    for (int index = 0; index < depth; ++index) {
        value.push_back('[');
    }
    value += "1";
    for (int index = 0; index < depth; ++index) {
        value.push_back(']');
    }
    return "items = " + value + "\n";
}

void expect_diagnostic_message(const auto& callable, const std::string& expected_message) {
    try {
        callable();
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError& error) {
        REQUIRE(error.diagnostic().message.find(expected_message) != std::string::npos);
    }
}

}

TEST_CASE(FuzzerRegression_template_lexer_rejects_truncated_string_escape) {
    prebyte::TemplateLexer lexer("{{-\"\\", "inline");
    REQUIRE_THROWS_AS(lexer.lex(), prebyte::DiagnosticError);
}

TEST_CASE(FuzzerRegression_template_parser_rejects_deep_expression_nesting) {
    std::string source = "{{ ";
    for (int index = 0; index < 100; ++index) {
        source.push_back('(');
    }
    source += "1";
    for (int index = 0; index < 100; ++index) {
        source.push_back(')');
    }
    source += " }}";

    prebyte::TemplateLexer lexer(source, "inline");
    prebyte::TemplateParser parser(lexer.lex());
    expect_diagnostic_message([&] { (void)parser.parse_document(); }, "Expression nesting is too deep");
}

TEST_CASE(FuzzerRegression_toml_parser_rejects_deep_table_path) {
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

TEST_CASE(FuzzerRegression_toml_parser_rejects_deep_array_nesting) {
    prebyte::TomlParser parser;
    REQUIRE_THROWS_AS(parser.parse_string(nested_toml_array(65)), std::runtime_error);
    parser.parse_string(nested_toml_array(32));
}

TEST_CASE(FuzzerRegression_compiled_template_serializer_rejects_truncated_blob) {
    prebyte::CompiledTemplateSerializer serializer;

    REQUIRE_THROWS_AS(serializer.deserialize("", "bad.pbc"), prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(serializer.deserialize("PBC", "bad.pbc"), prebyte::DiagnosticError);

    const std::string valid = serialize_minimal_program();
    REQUIRE_THROWS_AS(serializer.deserialize(valid.substr(0, valid.size() / 2), "bad.pbc"),
                      prebyte::DiagnosticError);
}

TEST_CASE(FuzzerRegression_compiled_template_serializer_rejects_oversized_string_length) {
    prebyte::CompiledTemplateSerializer serializer;
    std::string bytes;
    bytes.append("PBC1", 4);
    append_u32(bytes, 8);
    append_u32(bytes, 1000);

    REQUIRE_THROWS_AS(serializer.deserialize(bytes, "bad.pbc"), prebyte::DiagnosticError);
}

TEST_CASE(FuzzerRegression_compiled_template_serializer_rejects_oversized_section_count) {
    prebyte::CompiledTemplateSerializer serializer;
    const std::string bytes = make_compiled_header_with_counts(1000000);

    expect_diagnostic_message([&] { (void)serializer.deserialize(bytes, "bad.pbc"); },
                              "Invalid compiled template section size");
}

TEST_CASE(FuzzerRegression_include_resolver_rejects_overlong_and_overdeep_paths) {
    const std::filesystem::path root = resolver_test_root("include-limits");
    write_file(root / "main.txt", "Hello\n");

    prebyte::IncludeResolver resolver;
    prebyte::RenderSession session;
    session.include_anchor_root = root / "nested";
    prebyte::EffectiveSettings settings;
    settings.allow_includes = true;
    settings.include_paths.push_back(root);

    const std::string long_path(5000, 'a');
    expect_diagnostic_message([&] { (void)resolver.load(long_path, root / "nested" / "main.txt", settings, session); },
                              "Include path is too long");

    std::string deep_path;
    for (int index = 0; index < 100; ++index) {
        deep_path += "../";
    }
    deep_path += "main.txt";
    expect_diagnostic_message([&] { (void)resolver.load(deep_path, root / "nested" / "main.txt", settings, session); },
                              "Include path is too deep");
}

TEST_CASE(FuzzerRegression_include_resolver_rejects_traversal_outside_allowed_roots) {
    const std::filesystem::path root = resolver_test_root("include-traversal");
    const std::filesystem::path outside = root.parent_path() / (root.filename().string() + "-outside");
    write_file(outside / "secret.txt", "LEAKED\n");
    write_file(root / "nested" / "main.txt",
               "{{ include \"../../" + (root.filename().string() + "-outside") + "/secret.txt\" }}");

    prebyte::IncludeResolver resolver;
    prebyte::RenderSession session;
    session.include_anchor_root = root / "nested";
    prebyte::EffectiveSettings settings;
    settings.allow_includes = true;

    expect_diagnostic_message(
        [&] { (void)resolver.load("../../" + (root.filename().string() + "-outside") + "/secret.txt",
                                  root / "nested" / "main.txt", settings, session); },
        "escapes allowed roots");
}

TEST_CASE(FuzzerRegression_file_metadata_cache_treats_empty_path_as_missing) {
    prebyte::FileMetadataCache::instance().clear();
    const prebyte::FileMetadata metadata = prebyte::FileMetadataCache::instance().probe("");
    REQUIRE(!metadata.exists);
}

TEST_CASE(FuzzerRegression_lua_runtime_reads_sparse_lua_table_without_crashing) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    const prebyte::Value value = runtime.execute(
        R"(return { [2] = "Grace" })",
        prebyte::LuaChunkMode::InlineValue,
        settings,
        session,
        "runtime.txt",
        {});

    REQUIRE(value.is_object());
    REQUIRE(!value.is_list());
    REQUIRE(value.member("2").has_value());
    REQUIRE_EQ(value.member("2")->to_string(), std::string("Grace"));
}
