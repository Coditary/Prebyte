#include "TestHarness.h"

#include "config/RuleResolver.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "runtime/IncludeResolver.h"
#include "runtime/Renderer.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>

namespace {

struct RendererFixture {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator{builtins};
    prebyte::Renderer renderer{rule_resolver, include_resolver, evaluator};
};

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path function_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-function-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

prebyte::CompiledProgram roundtrip_compile(const std::string& source, const std::filesystem::path& path,
                                           const prebyte::EffectiveSettings& settings) {
    prebyte::CompiledTemplateCompiler compiler;
    prebyte::CompiledTemplateSerializer serializer;
    return serializer.deserialize(serializer.serialize(compiler.compile_source(source, path, path, settings)));
}

}

TEST_CASE(Renderer_native_function_works_in_set_and_if) {
    RendererFixture fixture;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    const std::string output = fixture.renderer.render_source(
        "{{ fn greet(name) }}Hello {{ name }}{{ endfn }}{{ set value = greet(\"Ada\") }}{{ if value == \"Hello Ada\" }}{{ value }}{{ else }}bad{{ endif }}",
        settings,
        "inline",
        session);

    REQUIRE_EQ(output, std::string("Hello Ada"));
}

TEST_CASE(Renderer_function_shadowing_is_local_to_nested_scope) {
    RendererFixture fixture;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    const std::string output = fixture.renderer.render_source(
        "{{ fn greet() }}A{{ endfn }}{{ if true }}{{ fn greet() }}B{{ endfn }}{{ greet() }}{{ endif }}{{ greet() }}",
        settings,
        "inline",
        session);

    REQUIRE_EQ(output, std::string("BA"));
}

TEST_CASE(Renderer_duplicate_function_definition_in_same_scope_fails) {
    RendererFixture fixture;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        fixture.renderer.render_source("{{ fn greet() }}A{{ endfn }}{{ fn greet() }}B{{ endfn }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_function_wrong_arity_fails) {
    RendererFixture fixture;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        fixture.renderer.render_source("{{ fn greet(name) }}{{ name }}{{ endfn }}{{ greet() }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_native_function_cannot_be_for_iterable) {
    RendererFixture fixture;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        fixture.renderer.render_source("{{ fn greet() }}hello{{ endfn }}{{ for item in greet() }}{{ item }}{{ endfor }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_lua_function_supports_member_access_and_condition) {
    RendererFixture fixture;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    const std::string output = fixture.renderer.render_source(
        "{{ fn user() lua:block }}return { name = \"Ada\", active = true }{{ endfn }}{{ if user().active }}{{ user().name }}{{ endif }}",
        settings,
        "inline",
        session);

    REQUIRE_EQ(output, std::string("Ada"));
}

TEST_CASE(Renderer_function_parameters_shadow_outer_variables) {
    RendererFixture fixture;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Outer");

    const std::string output = fixture.renderer.render_source(
        "{{ fn greet(name) }}{{ name }}{{ endfn }}{{ greet(\"Inner\") }}|{{ name }}",
        settings,
        "inline",
        session);

    REQUIRE_EQ(output, std::string("Inner|Outer"));
}

TEST_CASE(Renderer_recursive_function_hits_depth_limit) {
    RendererFixture fixture;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        fixture.renderer.render_source("{{ fn recurse() }}{{ recurse() }}{{ endfn }}{{ recurse() }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_function_source_and_compiled_program_match) {
    RendererFixture fixture;
    prebyte::EffectiveSettings settings;
    const std::filesystem::path path = "inline";
    const std::string source =
        "{{ fn users() lua:block }}return { { name = \"Ada\" }, { name = \"Grace\" } }{{ endfn }}"
        "{{ fn wrap(value) }}<{{ value }}>{{ endfn }}"
        "{{ for user in users() }}{{ wrap(user.name) }}{{ endfor }}|{{ __DATE__ == __DATE__ }}";

    prebyte::RenderSession source_session;
    const std::string source_output = fixture.renderer.render_source(source, settings, path, source_session);

    prebyte::CompiledProgram program = roundtrip_compile(source, path, settings);
    prebyte::RenderSession compiled_session;
    const std::string compiled_output = fixture.renderer.render_program(program, settings, path, compiled_session);

    REQUIRE_EQ(compiled_output, source_output);
}

TEST_CASE(Renderer_functions_defined_in_loop_scope_do_not_leak) {
    RendererFixture fixture;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set_value("items", prebyte::Value::list(prebyte::Data::Array{prebyte::Data("x")}));

    REQUIRE_THROWS_AS(
        fixture.renderer.render_source(
            "{{ for item in items }}{{ fn inner() }}{{ item }}{{ endfn }}{{ inner() }}{{ endfor }}{{ inner() }}",
            settings,
            "inline",
            session),
        prebyte::DiagnosticError);
}
