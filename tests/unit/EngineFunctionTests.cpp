#include "TestHarness.h"

#include "Engine.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>

namespace {

void write_engine_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path engine_function_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-engine-function-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

}

TEST_CASE(Engine_render_supports_template_functions_and_builtins) {
    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile(
        "{{ fn greet(name) }}Hello {{ name }} from {{ __OS__ }}{{ endfn }}{{ greet(\"Ada\") }}",
        "inline",
        "inline");

    const std::string output = engine.render(tpl);
    REQUIRE(output.find("Hello Ada from ") == 0);
}

TEST_CASE(Engine_render_supports_lua_function_iteration) {
    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile(
        "{{ fn users() lua:block }}return { { name = \"Ada\" }, { name = \"Grace\" } }{{ endfn }}{{ for user in users() }}{{ user.name }};{{ endfor }}",
        "inline",
        "inline");

    REQUIRE_EQ(engine.render(tpl), std::string("Ada;Grace;"));
}

TEST_CASE(Engine_render_to_matches_render_for_functions) {
    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile(
        "{{ fn greet(name) }}<{{ name }}>{{ endfn }}{{ greet(\"Ada\") }}{{ greet(\"Grace\") }}",
        "inline",
        "inline");

    const std::string rendered = engine.render(tpl);
    std::string streamed;
    engine.render_to(tpl, [&](std::string_view chunk) { streamed.append(chunk.data(), chunk.size()); });

    REQUIRE_EQ(streamed, rendered);
}

TEST_CASE(Engine_load_compiled_file_with_functions_renders) {
    const std::filesystem::path root = engine_function_root("load-compiled-function");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::string source = "{{ fn greet(name) }}Hello {{ name }}{{ endfn }}{{ greet(\"Ada\") }}";
    write_engine_file(source_path, source);

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source(source, source_path, source_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_engine_file(compiled_path, serializer.serialize(program));

    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.load_compiled_file(compiled_path);
    REQUIRE_EQ(engine.render(tpl), std::string("Hello Ada"));
}

TEST_CASE(Engine_render_recursive_function_throws) {
    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile("{{ fn recurse() }}{{ recurse() }}{{ endfn }}{{ recurse() }}", "inline", "inline");
    REQUIRE_THROWS_AS(engine.render(tpl), prebyte::DiagnosticError);
}
