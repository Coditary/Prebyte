#include "TestHarness.h"

#include "PrebyteEngine.h"
#include "io/InputBuffer.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>

namespace {

void write_engine_api_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path prebyte_engine_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-engine-api-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

prebyte::CompiledProgram compile_program(const std::string& source, const std::filesystem::path& path,
                                         const std::filesystem::path& logical_path) {
    prebyte::CompiledTemplateCompiler compiler;
    prebyte::CompiledTemplateSerializer serializer;
    return serializer.deserialize(serializer.serialize(
        compiler.compile_source(source, path, logical_path, prebyte::EffectiveSettings{})));
}

}

TEST_CASE(PrebyteEngine_applies_settings_profiles_rules_ignore_and_include_paths) {
    const std::filesystem::path root = prebyte_engine_test_root("settings-profile-api");
    const std::filesystem::path settings_path = root / "settings.json";
    write_engine_api_file(root / "cli-includes" / "cli-partial.pbt", "{{ fromSettings }}");
    write_engine_api_file(root / "profile-includes" / "profile-partial.pbt", "{{ fromProfile }}");
    write_engine_api_file(settings_path,
                          std::string("{\n")
                              + "  \"variables\": {\"fromSettings\": \"Settings\"},\n"
                              + "  \"rules\": {\"trim\": \"true\"},\n"
                              + "  \"profiles\": {\n"
                              + "    \"dev\": {\n"
                              + "      \"variables\": {\"fromProfile\": \"Profile\"},\n"
                              + "      \"include_paths\": [\"" + (root / "profile-includes").string() + "\"]\n"
                              + "    }\n"
                              + "  }\n"
                              + "}\n");

    prebyte::Prebyte engine(settings_path.string());
    engine.set_profile("dev");
    engine.add_include_path((root / "cli-includes").string());
    engine.set_variable("name", "  Ada  ");
    engine.set_variable("blank");
    engine.set_ignore("ignored");
    engine.set_rule("default_variable_value", "CLI");

    const std::string output = engine.process(
        "{{ include \"cli-partial\" }}|{{ include \"profile-partial\" }}|{{ name }}|X{{ blank }}Y|X{{ ignored }}Y|{{ missing }}");

    REQUIRE_EQ(output, std::string("Settings|Profile|Ada|XY|XY|CLI"));
}

TEST_CASE(PrebyteEngine_process_caches_static_inline_templates_and_writes_output) {
    const std::filesystem::path root = prebyte_engine_test_root("inline-cache");
    const std::filesystem::path output_path = root / "inline.txt";

    prebyte::Prebyte engine;
    engine.set_variable("name", "Ada");

    REQUIRE_EQ(engine.process("Hello {{ name }}"), std::string("Hello Ada"));
    REQUIRE_EQ(engine.process("Hello {{ name }}"), std::string("Hello Ada"));
    REQUIRE_EQ(engine.process("Hello {{ name }}"), std::string("Hello Ada"));

    engine.process("Hello {{ name }}", output_path.string());
    REQUIRE_EQ(prebyte::InputBuffer::from_file(output_path).view(), std::string_view("Hello Ada"));
}

TEST_CASE(PrebyteEngine_arguments_disable_inline_output_memoization) {
    prebyte::Prebyte engine;
    engine.add_argument("alpha");

    REQUIRE_EQ(engine.process("{{ ARGS[0] }}"), std::string("alpha"));
    REQUIRE_EQ(engine.process("{{ ARGS[0] }}"), std::string("alpha"));
    REQUIRE_EQ(engine.process("{{ ARGS[0] }}"), std::string("alpha"));
}

TEST_CASE(PrebyteEngine_process_file_uses_adjacent_compiled_template_and_caches_output) {
    const std::filesystem::path root = prebyte_engine_test_root("file-cache");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path output_path = root / "sample.out";
    write_engine_api_file(source_path, "Hello {{ name }}");

    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram program = compile_program("Hello {{ name }}", source_path, source_path);
    write_engine_api_file(serializer.compiled_path_for_source(source_path), serializer.serialize(program));

    prebyte::Prebyte engine;
    engine.set_variable("name", "Ada");

    REQUIRE_EQ(engine.process_file(source_path.string()), std::string("Hello Ada"));
    REQUIRE_EQ(engine.process_file(source_path.string()), std::string("Hello Ada"));
    REQUIRE_EQ(engine.process_file(source_path.string()), std::string("Hello Ada"));

    engine.process_file(source_path.string(), output_path.string());
    REQUIRE_EQ(prebyte::InputBuffer::from_file(output_path).view(), std::string_view("Hello Ada"));
}

TEST_CASE(PrebyteEngine_process_top_level_pbc_file) {
    const std::filesystem::path root = prebyte_engine_test_root("top-level-pbc");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::filesystem::path logical_path = root / "sample";
    write_engine_api_file(source_path, "Hello {{ name }}");

    const prebyte::CompiledProgram program = compile_program("Hello {{ name }}", source_path, logical_path);
    prebyte::CompiledTemplateSerializer serializer;
    write_engine_api_file(compiled_path, serializer.serialize(program));

    prebyte::Prebyte engine;
    engine.set_variable("name", "Ada");
    REQUIRE_EQ(engine.process_file(compiled_path.string()), std::string("Hello Ada"));
}

TEST_CASE(PrebyteEngine_dynamic_builtins_are_not_memoized) {
    prebyte::Prebyte engine;

    const std::string first = engine.process("{{ __UUID__ }}");
    const std::string second = engine.process("{{ __UUID__ }}");
    const std::string third = engine.process("{{ __UUID__ }}");

    REQUIRE(first != second);
    REQUIRE(second != third);
}

TEST_CASE(PrebyteEngine_new_limit_rules_apply_through_settings_profiles) {
    const std::filesystem::path root = prebyte_engine_test_root("new-limit-rules");
    const std::filesystem::path settings_path = root / "settings.json";
    write_engine_api_file(root / "partial.pbt", "x");
    write_engine_api_file(root / "main.pbt", "{{ include \"./partial\" }}");
    write_engine_api_file(settings_path,
                          std::string("{\n")
                              + "  \"profiles\": {\n"
                              + "    \"locked\": {\n"
                              + "      \"rules\": {\n"
                              + "        \"max_include_depth\": \"0\",\n"
                              + "        \"max_output_size_bytes\": \"1\"\n"
                              + "      }\n"
                              + "    }\n"
                              + "  }\n"
                              + "}\n");

    prebyte::Prebyte engine(settings_path.string());
    engine.set_profile("locked");
    REQUIRE_THROWS_AS(engine.process_file((root / "main.pbt").string()), prebyte::DiagnosticError);
}

TEST_CASE(PrebyteEngine_output_encoding_and_error_on_false_input_settings_apply_to_file_output_and_conditions) {
    const std::filesystem::path root = prebyte_engine_test_root("rule-behavior");
    const std::filesystem::path settings_path = root / "settings.json";
    const std::filesystem::path output_path = root / "out.txt";
    write_engine_api_file(settings_path,
                          std::string("{\n")
                              + "  \"rules\": {\n"
                              + "    \"output_encoding\": \"utf-16\",\n"
                              + "    \"error_on_false_input\": \"true\"\n"
                              + "  }\n"
                              + "}\n");

    prebyte::Prebyte engine(settings_path.string());
    try {
        static_cast<void>(engine.process("{{ if false }}bad{{ else }}ok{{ endif }}"));
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError& error) {
        REQUIRE_EQ(error.diagnostic().message, std::string("False input is not allowed in condition"));
    }

    engine.process("Hello", output_path.string());
    const std::string bytes = std::string(prebyte::InputBuffer::from_file(output_path).view());
    REQUIRE_EQ(bytes.size(), static_cast<std::size_t>(12));
    REQUIRE_EQ(static_cast<unsigned char>(bytes[0]), 0xFFu);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[1]), 0xFEu);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[2]), 0x48u);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[3]), 0x00u);
}
