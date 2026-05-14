#include "TestHarness.h"

#include "app/AppRunner.h"
#include "app/Command.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>

namespace {

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path app_runner_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-app-runner-feature-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

}

TEST_CASE(AppRunner_render_file_builtins_from_input_path) {
    const std::filesystem::path input_path = std::filesystem::temp_directory_path() / "prebyte-builtins" / "demo.pbt";

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = input_path;
    command.inline_input = "{{ __FILENAME__ }}|{{ __EXTENSION__ }}|{{ __DIR__ }}";

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("demo|pbt|") == 0);
}

TEST_CASE(AppRunner_render_top_level_pbc_file_with_functions_and_builtins) {
    const std::filesystem::path root = app_runner_root("top-level-pbc-functions");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::string source =
        "{{ fn greet(name) }}Hello {{ name }} from {{ __OS__ }}{{ endfn }}{{ greet(\"Ada\") }}";

    write_file(source_path, source);
    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source(source, source_path, source_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(compiled_path, serializer.serialize(program));

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = compiled_path;

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE(output.find("Hello Ada from ") == 0);
}

TEST_CASE(AppRunner_function_wrong_arity_fails) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ fn greet(name) }}{{ name }}{{ endfn }}{{ greet() }}";

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_render_functions_with_structured_imports_and_loop) {
    const std::filesystem::path root = app_runner_root("functions-structured-imports");
    const std::filesystem::path items_path = root / "items.yaml";
    write_file(items_path, "- Ada\n- Grace\n");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input =
        "{{ fn wrap(name) }}<{{ name }}>{{ endfn }}"
        "{{ for item in items }}{{ wrap(item) }}{{ endfor }}";
    command.define_args = {"items=@" + items_path.string()};

    prebyte::AppRunner runner;
    REQUIRE_EQ(runner.execute(command), std::string("<Ada><Grace>"));
}
