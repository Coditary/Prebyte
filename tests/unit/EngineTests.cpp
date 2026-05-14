#include "TestHarness.h"

#include "Engine.h"
#include "datatypes/Data.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>

namespace {

void write_engine_test_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path engine_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-engine-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

}

TEST_CASE(Engine_render_matches_render_to_for_plain_interpolation) {
    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile("Hello {{ name }}");

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");

    const std::string rendered = engine.render(tpl, ctx);

    std::string streamed;
    std::size_t chunk_count = 0;
    engine.render_to(tpl, [&](std::string_view chunk) {
        ++chunk_count;
        streamed.append(chunk.data(), chunk.size());
    }, ctx);

    REQUIRE_EQ(streamed, rendered);
    REQUIRE(chunk_count > 1);
}

TEST_CASE(Engine_render_supports_structured_values_loops_and_includes) {
    const std::filesystem::path root = engine_test_root("structured-loops-includes");
    write_engine_test_file(root / "partial.pbt", "<{{ loop.index }}:{{ item }}>");

    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile(
        "A{{ for item in items }}{{ include \"./partial\" }}{{ endfor }}Z",
        root / "main.pbt",
        root / "main.pbt");

    prebyte::RenderContext ctx;
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    ctx.set("items", prebyte::Value::list(std::move(items)));

    REQUIRE_EQ(engine.render(tpl, ctx), std::string("A<1:Ada><2:Grace>Z"));
}

TEST_CASE(Engine_render_supports_args_and_lua) {
    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile("{{ ARGS[1] }} {{ lua \"return upper(name)\" }}");

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");
    ctx.set_args({"zero", "one"});

    REQUIRE_EQ(engine.render(tpl, ctx), std::string("one ADA"));
}

TEST_CASE(Engine_compile_honors_compile_options) {
    prebyte::Engine engine;
    prebyte::CompileOptions options;
    options.variable_prefix = "<<";
    options.variable_suffix = ">>";

    const prebyte::CompiledTemplate tpl = engine.compile("Hello << name >>", {}, {}, options);

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");

    REQUIRE_EQ(engine.render(tpl, ctx), std::string("Hello Ada"));
}

TEST_CASE(Engine_compile_file_uses_path_for_source_and_logical_path) {
    const std::filesystem::path root = engine_test_root("compile-file");
    const std::filesystem::path path = root / "sample.pbt";
    write_engine_test_file(path, "Hello {{ name }}");

    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile_file(path);

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");

    REQUIRE_EQ(tpl.source_path(), path);
    REQUIRE_EQ(tpl.logical_path(), path);
    REQUIRE_EQ(engine.render(tpl, ctx), std::string("Hello Ada"));
}

TEST_CASE(Engine_render_to_keeps_partial_output_before_error) {
    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile("Hello {{ name }} {{ user }}");

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");
    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");
    ctx.set("user", prebyte::Value::object(std::move(user)));

    std::string output;
    bool threw = false;
    try {
        engine.render_to(tpl, [&](std::string_view chunk) {
            output.append(chunk.data(), chunk.size());
        }, ctx);
    } catch (const prebyte::DiagnosticError&) {
        threw = true;
    }

    REQUIRE(threw);
    REQUIRE_EQ(output, std::string("Hello Ada "));
}

TEST_CASE(Engine_render_empty_compiled_template_fails) {
    prebyte::Engine engine;
    prebyte::CompiledTemplate tpl;

    REQUIRE_THROWS_AS(engine.render(tpl), prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(engine.render_to(tpl, [](std::string_view) {}), prebyte::DiagnosticError);
}

TEST_CASE(Engine_render_honors_render_options) {
    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile("Hello {{ missing }}");

    prebyte::RenderOptions opts;
    opts.settings.strict_variables = true;

    REQUIRE_THROWS_AS(engine.render(tpl, {}, opts), prebyte::DiagnosticError);
}

TEST_CASE(Engine_render_supports_filters_set_and_trim_tags) {
    const std::filesystem::path root = engine_test_root("filters-set-trim");
    write_engine_test_file(root / "partial.pbt", "{{ value }}");

    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile(
        "{{ set value = name | trim | upper }}A {{- include \"./partial\" -}} B{{ if value == \"ADA\" }}{{ missing | default(\"!\") }}{{ endif }}",
        root / "main.pbt",
        root / "main.pbt");

    prebyte::RenderContext ctx;
    ctx.set("name", "  Ada  ");

    REQUIRE_EQ(engine.render(tpl, ctx), std::string("AADAB!"));
}

TEST_CASE(Engine_render_honors_false_input_rule) {
    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile("{{ if false }}bad{{ else }}ok{{ endif }}");

    prebyte::RenderOptions opts;
    opts.settings.error_on_false_input = true;

    try {
        static_cast<void>(engine.render(tpl, {}, opts));
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError& error) {
        REQUIRE_EQ(error.diagnostic().message, std::string("False input is not allowed in condition"));
    }
}
