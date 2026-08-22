#include "TestHarness.h"

#include "app/BatchProcessor.h"
#include "app/Command.h"
#include "io/InputBuffer.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>

namespace {

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path batch_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-batch-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

prebyte::Command make_batch_command(const std::filesystem::path& root, const std::string& batch_json,
                                    const std::string& template_source = "{{ value }}") {
    write_file(root / "template.txt", template_source);
    write_file(root / "data.json", batch_json);

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = root / "template.txt";
    command.batch_path = root / "data.json";
    return command;
}

}

TEST_CASE(BatchProcessor_rejects_invalid_batch_configuration) {
    const std::filesystem::path root = batch_test_root("invalid-config");
    write_file(root / "template.txt", "{{ value }}");
    write_file(root / "data.json", R"([{"value":"one"}])");

    prebyte::BatchProcessor processor;

    prebyte::Command missing_batch;
    missing_batch.mode = prebyte::CommandMode::Render;
    missing_batch.input_path = root / "template.txt";
    REQUIRE_THROWS_AS(processor.execute(missing_batch), prebyte::DiagnosticError);

    prebyte::Command stdin_without_template;
    stdin_without_template.mode = prebyte::CommandMode::Render;
    stdin_without_template.batch_from_stdin = true;
    REQUIRE_THROWS_AS(processor.execute(stdin_without_template), prebyte::DiagnosticError);

    prebyte::Command missing_batch_file = make_batch_command(root, R"([{"value":"one"}])");
    missing_batch_file.batch_path = root / "missing.json";
    REQUIRE_THROWS_AS(processor.execute(missing_batch_file), prebyte::DiagnosticError);
}

TEST_CASE(BatchProcessor_rejects_invalid_batch_payloads) {
    prebyte::BatchProcessor processor;

    REQUIRE_THROWS_AS(processor.execute(make_batch_command(batch_test_root("bad-json"), "{ not json")),
                      std::runtime_error);
    REQUIRE_THROWS_AS(processor.execute(make_batch_command(batch_test_root("empty-array"), "[]")),
                      prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(processor.execute(make_batch_command(batch_test_root("empty-object"), "{}")),
                      prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(processor.execute(make_batch_command(batch_test_root("scalar-root"), R"("nope")")),
                      prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(
        processor.execute(make_batch_command(batch_test_root("array-non-object"), R"(["bad"])")),
        prebyte::DiagnosticError);
    REQUIRE_THROWS_AS(
        processor.execute(make_batch_command(batch_test_root("object-non-object"), R"({"entry":"bad"})")),
        prebyte::DiagnosticError);
}

TEST_CASE(BatchProcessor_rejects_invalid_output_targets) {
    const std::filesystem::path root = batch_test_root("invalid-output");
    prebyte::Command command = make_batch_command(root, R"([{"value":"one"},{"value":"two"}])");
    command.output_path = root / "single.txt";

    prebyte::BatchProcessor processor;
    REQUIRE_THROWS_AS(processor.execute(command), prebyte::DiagnosticError);

    prebyte::Command bad_override = make_batch_command(batch_test_root("bad-override"),
                                                       R"([{"$output":42,"value":"one"}])");
    REQUIRE_THROWS_AS(processor.execute(bad_override), prebyte::DiagnosticError);
}

TEST_CASE(BatchProcessor_execute_combines_stdout_output) {
    const std::filesystem::path root = batch_test_root("stdout");
    prebyte::Command command = make_batch_command(root, R"([{"value":"one"},{"value":"two"}])");

    prebyte::BatchProcessor processor;
    REQUIRE_EQ(processor.execute(command), std::string("onetwo"));
}

TEST_CASE(BatchProcessor_execute_writes_single_output_file) {
    const std::filesystem::path root = batch_test_root("single-file");
    prebyte::Command command = make_batch_command(root, R"([{"value":"only"}])");
    command.output_path = root / "result.txt";

    prebyte::BatchProcessor processor;
    REQUIRE(processor.execute(command).empty());
    REQUIRE_EQ(std::string(prebyte::InputBuffer::from_file(root / "result.txt").view()), std::string("only"));
}

TEST_CASE(BatchProcessor_execute_writes_directory_outputs) {
    const std::filesystem::path root = batch_test_root("directory-output");
    prebyte::Command command = make_batch_command(root, R"([{"value":"one"},{"value":"two"}])");
    command.output_path = root / "out/";

    prebyte::BatchProcessor processor;
    REQUIRE(processor.execute(command).empty());
    REQUIRE_EQ(std::string(prebyte::InputBuffer::from_file(root / "out" / "0.txt").view()), std::string("one"));
    REQUIRE_EQ(std::string(prebyte::InputBuffer::from_file(root / "out" / "1.txt").view()), std::string("two"));
}

TEST_CASE(BatchProcessor_execute_honors_output_override_and_object_keys) {
    const std::filesystem::path root = batch_test_root("output-names");
    prebyte::Command command = make_batch_command(root,
                                                  R"([
        {"$output":"custom.txt","value":"one"},
        {"_output":"second.txt","value":"two"},
        {"value":"three"}
    ])");
    command.output_path = root / "out/";

    prebyte::BatchProcessor processor;
    processor.execute(command);

    REQUIRE_EQ(std::string(prebyte::InputBuffer::from_file(root / "out" / "custom.txt").view()), std::string("one"));
    REQUIRE_EQ(std::string(prebyte::InputBuffer::from_file(root / "out" / "second.txt").view()), std::string("two"));
    REQUIRE_EQ(std::string(prebyte::InputBuffer::from_file(root / "out" / "2.txt").view()), std::string("three"));

    prebyte::Command object_keys = make_batch_command(batch_test_root("object-keys"),
                                                      R"({"first.txt":{"value":"alpha"},"second.txt":{"value":"beta"}})");
    object_keys.output_path = root / "object-out/";
    processor.execute(object_keys);

    REQUIRE_EQ(std::string(prebyte::InputBuffer::from_file(root / "object-out" / "first.txt").view()),
               std::string("alpha"));
    REQUIRE_EQ(std::string(prebyte::InputBuffer::from_file(root / "object-out" / "second.txt").view()),
               std::string("beta"));
}

TEST_CASE(BatchProcessor_execute_applies_scalar_and_structured_variables) {
    const std::filesystem::path root = batch_test_root("variables");
    prebyte::Command command = make_batch_command(
        root,
        R"([{
            "name":"Ada",
            "active":true,
            "count":2,
            "user":{"role":"admin"},
            "tags":["a","b"]
        }])",
        R"({{ name }}|{{ active }}|{{ count }}|{{ user.role }}|{{ tags[0] }})");

    prebyte::BatchProcessor processor;
    REQUIRE_EQ(processor.execute(command), std::string("Ada|true|2|admin|a"));
}

TEST_CASE(BatchProcessor_execute_includes_benchmark_suffix) {
    const std::filesystem::path root = batch_test_root("benchmark");
    prebyte::Command command = make_batch_command(root, R"([{"value":"timed"}])");
    command.benchmark = true;

    prebyte::BatchProcessor processor;
    const std::string output = processor.execute(command);
    REQUIRE(output.find("timed") == 0);
    REQUIRE(output.find("\n[benchmark] ") != std::string::npos);
    REQUIRE(output.find("lua_cache_hits=") != std::string::npos);
    REQUIRE(output.find("lua_cache_misses=") != std::string::npos);
}
