#include "TestHarness.h"

#include "app/AppRunner.h"
#include "app/Command.h"
#include "config/RuleResolver.h"
#include "runtime/expression/BuiltinRegistry.h"
#include "runtime/compiled/CompiledTemplateCache.h"
#include "runtime/compiled/CompiledTemplateCompiler.h"
#include "runtime/compiled/CompiledTemplateSerializer.h"
#include "runtime/expression/ExpressionEvaluator.h"
#include "runtime/cache/FileMetadataCache.h"
#include "runtime/resolution/IncludeResolver.h"
#include "runtime/render/Renderer.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>

namespace {

struct RoundtripHarness {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator{builtins};
    prebyte::Renderer renderer{rule_resolver, include_resolver, evaluator};
    prebyte::CompiledTemplateCompiler compiler;
    prebyte::CompiledTemplateSerializer serializer;
};

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::filesystem::path roundtrip_test_root(const std::string& name) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "prebyte-compiled-roundtrip-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

prebyte::RenderSession make_session(const std::string& name = "Ada", const std::string& enabled = "true") {
    prebyte::RenderSession session;
    session.variables.set("name", name);
    session.variables.set("enabled", enabled);
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));
    return session;
}

std::string render_direct(RoundtripHarness& harness, const std::string& source,
                          const std::filesystem::path& current_file, prebyte::RenderSession session,
                          const prebyte::EffectiveSettings& settings) {
    return harness.renderer.render_source(source, settings, current_file, session);
}

std::string render_serialized_program(RoundtripHarness& harness, const prebyte::CompiledProgram& program,
                                      prebyte::RenderSession session, const prebyte::EffectiveSettings& settings) {
    return harness.renderer.render_program(program, settings, program.logical_path, session);
}

prebyte::CompiledProgram serialize_roundtrip(RoundtripHarness& harness, const prebyte::CompiledProgram& program) {
    return harness.serializer.deserialize(harness.serializer.serialize(program));
}

void require_roundtrip_matches_direct(RoundtripHarness& harness, const std::string& source,
                                      const std::filesystem::path& current_file, prebyte::RenderSession session,
                                      const prebyte::EffectiveSettings& settings) {
    const std::string direct_output = render_direct(harness, source, current_file, session, settings);

    const prebyte::CompiledProgram compiled =
        harness.compiler.compile_source(source, current_file, current_file, settings);
    const prebyte::CompiledProgram once = serialize_roundtrip(harness, compiled);
    const prebyte::CompiledProgram twice = serialize_roundtrip(harness, once);

    const std::string once_output = render_serialized_program(harness, once, make_session(), settings);
    const std::string twice_output = render_serialized_program(harness, twice, make_session(), settings);

    REQUIRE_EQ(once_output, direct_output);
    REQUIRE_EQ(twice_output, direct_output);
}

void reset_template_caches(const std::filesystem::path& source_path, const prebyte::EffectiveSettings& settings) {
    prebyte::FileMetadataCache::instance().clear();
    prebyte::CompiledTemplateSerializer serializer;
    prebyte::CompiledTemplateCache::instance().erase(serializer.compiled_path_for_source(source_path), settings);
}

std::string app_runner_render_file(const std::filesystem::path& input_path,
                                   const std::vector<std::string>& define_args = {"name=Ada", "enabled=true"}) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = input_path;
    command.define_args = define_args;

    prebyte::AppRunner runner;
    return runner.execute(command);
}

void write_compiled_artifact(const std::filesystem::path& source_path, const std::string& source,
                             const prebyte::EffectiveSettings& settings) {
    prebyte::CompiledTemplateCompiler compiler;
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram program =
        compiler.compile_source(source, source_path, source_path, settings);
    write_file(serializer.compiled_path_for_source(source_path), serializer.serialize(program));
}

}

TEST_CASE(CompiledTemplateRoundtrip_simple_template_matches_direct_render) {
    RoundtripHarness harness;
    prebyte::EffectiveSettings settings;
    const std::filesystem::path current_file = "roundtrip-simple.pbt";
    const std::string source = "Hello {{ name }}";

    require_roundtrip_matches_direct(harness, source, current_file, make_session(), settings);
}

TEST_CASE(CompiledTemplateRoundtrip_control_flow_and_loops_match_direct_render) {
    RoundtripHarness harness;
    prebyte::EffectiveSettings settings;
    settings.include_paths.push_back(roundtrip_test_root("loop-include"));
    const std::filesystem::path root = settings.include_paths.back();
    const std::filesystem::path current_file = root / "main.pbt";
    write_file(root / "partial.pbt", "<{{ loop.index }}:{{ item }}>");

    const std::string source =
        "{{ if enabled }}Y{{ for item in items }}{{ include \"partial.pbt\" }}{{ endfor }}{{ else }}N{{ endif }}";

    require_roundtrip_matches_direct(harness, source, current_file, make_session(), settings);
}

TEST_CASE(CompiledTemplateRoundtrip_lua_and_functions_match_direct_render) {
    RoundtripHarness harness;
    prebyte::EffectiveSettings settings;
    const std::filesystem::path current_file = "roundtrip-lua.pbt";
    const std::string source =
        "{{ fn greet(name) }}Hello {{ name }}{{ endfn }}"
        "{{ if lua(\"return starts_with(name, 'Ada')\") }}{{ greet(name) }}{{ else }}bad{{ endif }}";

    require_roundtrip_matches_direct(harness, source, current_file, make_session(), settings);
}

TEST_CASE(CompiledTemplateRoundtrip_try_load_valid_reads_adjacent_pbc) {
    const std::filesystem::path root = roundtrip_test_root("try-load-valid");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::string source = "Hello {{ name }}";
    write_file(source_path, source);

    prebyte::EffectiveSettings settings;
    write_compiled_artifact(source_path, source, settings);

    reset_template_caches(source_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram* loaded =
        serializer.try_load_valid(serializer.compiled_path_for_source(source_path), settings);
    REQUIRE(loaded != nullptr);

    RoundtripHarness harness;
    const std::string cached_output =
        render_serialized_program(harness, *loaded, make_session(), settings);
    const std::string direct_output =
        render_direct(harness, source, source_path, make_session(), settings);

    REQUIRE_EQ(cached_output, std::string("Hello Ada"));
    REQUIRE_EQ(cached_output, direct_output);
}

TEST_CASE(CompiledTemplateRoundtrip_app_runner_pbc_matches_source_render) {
    const std::filesystem::path root = roundtrip_test_root("app-runner-parity");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::string source =
        "{{ include \"header.txt\" }}{{ if enabled }}Enabled{{ else }}Disabled{{ endif }}\nFooter\n";
    write_file(root / "header.txt", "Header {{ name }}\n");
    write_file(source_path, source);

    prebyte::EffectiveSettings settings;
    settings.allow_includes = true;
    settings.include_paths.push_back(root);
    write_compiled_artifact(source_path, source, settings);
    REQUIRE(std::filesystem::exists(compiled_path));

    const std::string from_source = app_runner_render_file(source_path);
    const std::string from_compiled = app_runner_render_file(compiled_path);

    REQUIRE_EQ(from_source, std::string("Header Ada\nEnabled\nFooter\n"));
    REQUIRE_EQ(from_compiled, from_source);
}

TEST_CASE(CompiledTemplateRoundtrip_modified_source_recompiles_when_dependency_is_stale) {
    const std::filesystem::path root = roundtrip_test_root("dependency-stale");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::string initial_source = "Hello {{ name }}";
    const std::string updated_source = "Hi {{ name }}";
    write_file(source_path, initial_source);

    prebyte::EffectiveSettings settings;
    write_compiled_artifact(source_path, initial_source, settings);

    reset_template_caches(source_path, settings);
    REQUIRE_EQ(app_runner_render_file(source_path), std::string("Hello Ada"));
    REQUIRE_EQ(app_runner_render_file(compiled_path), std::string("Hello Ada"));

    write_file(source_path, updated_source);

    RoundtripHarness harness;
    const prebyte::CompiledProgram stale_program =
        harness.serializer.deserialize(read_file(compiled_path), compiled_path);
    REQUIRE_EQ(render_serialized_program(harness, stale_program, make_session(), settings), std::string("Hello Ada"));

    reset_template_caches(source_path, settings);
    REQUIRE_EQ(app_runner_render_file(source_path), std::string("Hi Ada"));

    write_compiled_artifact(source_path, updated_source, settings);

    reset_template_caches(source_path, settings);
    REQUIRE_EQ(app_runner_render_file(source_path), std::string("Hi Ada"));
    REQUIRE_EQ(app_runner_render_file(compiled_path), std::string("Hi Ada"));
}

TEST_CASE(CompiledTemplateRoundtrip_on_disk_bytes_match_in_memory_serialization) {
    const std::filesystem::path root = roundtrip_test_root("on-disk-bytes");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::string source = "{{ for item in items }}{{ item }};{{ endfor }}";
    write_file(source_path, source);

    RoundtripHarness harness;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram compiled =
        harness.compiler.compile_source(source, source_path, source_path, settings);
    const std::string bytes = harness.serializer.serialize(compiled);
    write_file(root / "sample.pbc", bytes);

    const prebyte::CompiledProgram loaded_from_disk =
        harness.serializer.deserialize(bytes, root / "sample.pbc");

    const std::string direct_output = render_direct(harness, source, source_path, make_session(), settings);
    const std::string disk_output =
        render_serialized_program(harness, loaded_from_disk, make_session(), settings);

    REQUIRE_EQ(disk_output, std::string("Ada;Grace;"));
    REQUIRE_EQ(disk_output, direct_output);
}
