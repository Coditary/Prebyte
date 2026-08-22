#include "TestHarness.h"

#include "CliProcess.h"

#include "io/InputBuffer.h"
#include "support/Version.h"

#include <filesystem>
#include <fstream>

namespace {

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path cli_test_root(const std::string& name) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "prebyte-cli-binary-e2e" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

std::string read_file(const std::filesystem::path& path) {
    return std::string(prebyte::InputBuffer::from_file(path).view());
}

}

TEST_CASE(CliBinaryE2E_help_exits_zero_and_prints_usage) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli({"--help"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find("prebyte [input] [options]") != std::string::npos);
    REQUIRE(result.stdout_text.find("list rules|vars|profiles|ignore|ignores") != std::string::npos);
    REQUIRE(result.stderr_text.empty());
}

TEST_CASE(CliBinaryE2E_version_exits_zero_and_prints_version) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli({"--version"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find(std::string(prebyte::VERSION)) != std::string::npos);
    REQUIRE(result.stderr_text.empty());
}

TEST_CASE(CliBinaryE2E_render_simple_fixture_to_stdout) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {"tests/fixtures/render_simple/input.txt", "-Dname=Ada"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE_EQ(result.stdout_text, std::string("Hello Ada\n"));
    REQUIRE(result.stderr_text.empty());
}

TEST_CASE(CliBinaryE2E_render_include_fixture_with_define_flags) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {"tests/fixtures/render_include_if/input.txt", "-Dname=Ada", "-Denabled=true"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find("Header for Ada") != std::string::npos);
    REQUIRE(result.stdout_text.find("Enabled") != std::string::npos);
    REQUIRE(result.stdout_text.find("Footer") != std::string::npos);
}

TEST_CASE(CliBinaryE2E_render_writes_output_file) {
    const std::filesystem::path root = cli_test_root("output-file");
    const std::filesystem::path output_path = root / "out.txt";

    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {"tests/fixtures/render_simple/input.txt", "-Dname=Grace", "-o", output_path.string()});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.empty());
    REQUIRE(std::filesystem::exists(output_path));
    REQUIRE_EQ(read_file(output_path), std::string("Hello Grace\n"));
}

TEST_CASE(CliBinaryE2E_list_rules_applies_settings_and_profile_flags) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {"list", "rules", "-s", "tests/fixtures/settings_profile_merge/settings.yaml", "-p", "friendly", "-r",
         "trim=true", "-r", ".md::default_variable_value=Fallback"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find("trim=true") != std::string::npos);
    REQUIRE(result.stdout_text.find("extension:.md::default_variable_value=Fallback") != std::string::npos);
}

TEST_CASE(CliBinaryE2E_list_vars_applies_settings_profile_and_defines) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {"list", "vars", "-s", "tests/fixtures/settings_profile_merge/settings.yaml", "-p", "friendly",
         "-Dname=Ada"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find("greeting=Hi") != std::string::npos);
    REQUIRE(result.stdout_text.find("name=Ada") != std::string::npos);
}

TEST_CASE(CliBinaryE2E_explain_lua_topic_prints_helpers_and_limits) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli({"--explain", "lua"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find("upper(value)") != std::string::npos);
    REQUIRE(result.stdout_text.find("lua_instruction_limit=100000") != std::string::npos);
}

TEST_CASE(CliBinaryE2E_unknown_argument_exits_nonzero_with_diagnostic) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli({"--wat"});

    REQUIRE(result.exit_code != 0);
    REQUIRE(result.stderr_text.find("error[CLI001]") != std::string::npos);
    REQUIRE(result.stderr_text.find("Unknown argument") != std::string::npos);
}

TEST_CASE(CliBinaryE2E_strict_variable_failure_exits_nonzero) {
    const prebyte::test::ProcessResult result =
        prebyte::test::run_cli({"-r", "strict_variables=true", "tests/fixtures/render_simple/input.txt"});

    REQUIRE(result.exit_code != 0);
    REQUIRE(result.stderr_text.find("error[") != std::string::npos);
}

TEST_CASE(CliBinaryE2E_allow_includes_disabled_exits_nonzero) {
    const std::filesystem::path root = cli_test_root("includes-disabled");
    write_file(root / "partial.pbt", "Partial\n");
    write_file(root / "main.pbt", "{{ include \"partial.pbt\" }}");

    const prebyte::test::ProcessResult result =
        prebyte::test::run_cli({(root / "main.pbt").string(), "-r", "allow_includes=false"});

    REQUIRE(result.exit_code != 0);
    REQUIRE(result.stderr_text.find("error[") != std::string::npos);
}

TEST_CASE(CliBinaryE2E_list_profiles_via_subprocess) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {"list", "profiles", "-s", "tests/fixtures/settings_profile_merge/settings.yaml"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find("friendly") != std::string::npos);
}

TEST_CASE(CliBinaryE2E_render_with_settings_file_via_subprocess) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {"tests/fixtures/settings_profile_merge/input.txt", "-s",
         "tests/fixtures/settings_profile_merge/settings.yaml", "-p", "friendly", "-Dname=Ada"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find("Ada") != std::string::npos);
}

TEST_CASE(CliBinaryE2E_render_named_structured_imports_via_cli) {
    const std::filesystem::path root = cli_test_root("structured-imports");
    const std::filesystem::path template_path = root / "input.pbt";
    const std::filesystem::path user_path = root / "user.json";
    write_file(template_path, "{{ user.name }}\n");
    write_file(user_path, R"({"name":"Ada"})");

    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {template_path.string(), "-Duser=@" + user_path.string()});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE_EQ(result.stdout_text, std::string("Ada\n"));
}

TEST_CASE(CliBinaryE2E_batch_render_from_file_writes_stdout) {
    const std::filesystem::path root = cli_test_root("batch-stdout");
    write_file(root / "template.txt", "Hello {{ name }}!\n");
    write_file(root / "data.json", R"([{"name":"Ada"},{"name":"Grace"}])");

    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {(root / "template.txt").string(), "--batch", (root / "data.json").string()});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE_EQ(result.stdout_text, std::string("Hello Ada!\nHello Grace!\n"));
}

TEST_CASE(CliBinaryE2E_batch_render_writes_directory_outputs) {
    const std::filesystem::path root = cli_test_root("batch-directory");
    write_file(root / "template.txt", "{{ value }}");
    write_file(root / "data.json", R"({"first.txt":{"value":"one"},"second.txt":{"value":"two"}})");

    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {(root / "template.txt").string(), "--batch", (root / "data.json").string(), "-o",
         (root / "out").string()});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE_EQ(read_file(root / "out" / "first.txt"), std::string("one"));
    REQUIRE_EQ(read_file(root / "out" / "second.txt"), std::string("two"));
}

TEST_CASE(CliBinaryE2E_render_args_are_exposed_as_args_index) {
    const std::filesystem::path root = cli_test_root("render-args");
    const std::filesystem::path template_path = root / "template.txt";
    write_file(template_path, "{{ ARGS[0] }}|{{ ARGS[1] }}\n");

    const prebyte::test::ProcessResult result =
        prebyte::test::run_cli({template_path.string(), "alpha", "beta"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE_EQ(result.stdout_text, std::string("alpha|beta\n"));
}

TEST_CASE(CliBinaryE2E_benchmark_flag_appends_timing_suffix) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {"tests/fixtures/render_simple/input.txt", "-Dname=Ada", "--benchmark"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find("Hello Ada\n") == 0);
    REQUIRE(result.stdout_text.find("\n[benchmark] ") != std::string::npos);
    REQUIRE(result.stdout_text.find("lua_cache_hits=") != std::string::npos);
}

TEST_CASE(CliBinaryE2E_list_ignores_merges_settings_profiles_and_cli) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {"list", "ignores", "-s", "tests/fixtures/settings_profile_merge/settings.yaml", "-p", "friendly",
         "-i", "cli_only"});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE(result.stdout_text.find("base_ignore") != std::string::npos);
    REQUIRE(result.stdout_text.find("friendly_ignore") != std::string::npos);
    REQUIRE(result.stdout_text.find("cli_only") != std::string::npos);
}

TEST_CASE(CliBinaryE2E_render_from_stdin_with_render_args) {
    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {"--", "alpha", "beta"}, prebyte::test::cli_working_directory(), {},
        "Hello {{ ARGS[0] }}|{{ ARGS[1] }}\n");

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE_EQ(result.stdout_text, std::string("Hello alpha|beta\n"));
}

TEST_CASE(CliBinaryE2E_utf16_output_writes_bom_and_little_endian_units) {
    const std::filesystem::path root = cli_test_root("utf16-output");
    const std::filesystem::path output_path = root / "out.txt";

    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {"tests/fixtures/render_simple/input.txt", "-Dname=A", "-r", "output_encoding=utf-16", "-o",
         output_path.string()});

    REQUIRE_EQ(result.exit_code, 0);
    const std::string bytes = read_file(output_path);
    REQUIRE_EQ(bytes.size(), static_cast<std::size_t>(18));
    REQUIRE_EQ(static_cast<unsigned char>(bytes[0]), 0xFFu);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[1]), 0xFEu);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[2]), 0x48u);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[3]), 0x00u);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[16]), 0x0Au);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[17]), 0x00u);
}

TEST_CASE(CliBinaryE2E_include_path_allows_parent_relative_includes) {
    const std::filesystem::path root = cli_test_root("include-path");
    write_file(root / "partial.pbt", "Partial\n");
    write_file(root / "nested" / "main.pbt", "{{ include \"../partial.pbt\" }}");

    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {(root / "nested" / "main.pbt").string(), "-r", "allow_includes=true", "-I", root.string()});

    REQUIRE_EQ(result.exit_code, 0);
    REQUIRE_EQ(result.stdout_text, std::string("Partial\n"));
}

TEST_CASE(CliBinaryE2E_traversal_outside_allowed_roots_exits_nonzero) {
    const std::filesystem::path root = cli_test_root("traversal-escape");
    const std::filesystem::path outside = root.parent_path() / (root.filename().string() + "-outside");
    write_file(outside / "secret.txt", "LEAKED\n");
    write_file(root / "nested" / "main.pbt",
               "{{ include \"../../" + (root.filename().string() + "-outside") + "/secret.txt\" }}");

    const prebyte::test::ProcessResult result = prebyte::test::run_cli(
        {(root / "nested" / "main.pbt").string(), "-r", "allow_includes=true"});

    REQUIRE(result.exit_code != 0);
    REQUIRE(result.stderr_text.find("escapes allowed roots") != std::string::npos);
}
