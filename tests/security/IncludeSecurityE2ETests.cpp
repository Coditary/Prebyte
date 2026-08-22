#include "TestHarness.h"

#include "app/AppRunner.h"
#include "app/Command.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path security_test_root(const std::string& name) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "prebyte-include-security-e2e" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

std::string render_file(const std::filesystem::path& input_path,
                        const std::vector<std::string>& rule_args = {},
                        const std::vector<std::filesystem::path>& include_paths = {}) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = input_path;
    command.rule_args = rule_args;
    command.include_paths = include_paths;
    prebyte::AppRunner runner;
    return runner.execute(command);
}

void expect_render_file_error(const std::filesystem::path& input_path,
                              const std::vector<std::string>& rule_args = {},
                              const std::string& message_fragment = {}) {
    try {
        static_cast<void>(render_file(input_path, rule_args));
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError& error) {
        if (!message_fragment.empty()
            && error.diagnostic().message.find(message_fragment) == std::string::npos) {
            throw std::runtime_error("unexpected diagnostic: " + error.diagnostic().message);
        }
    }
}

}

TEST_CASE(IncludeSecurityE2E_safe_relative_include_within_template_root_works) {
    const std::filesystem::path root = security_test_root("safe-relative");
    write_file(root / "header.txt", "Header\n");
    write_file(root / "main.pbt", "{{ include \"header.txt\" }}Body\n");

    REQUIRE_EQ(render_file(root / "main.pbt", {"allow_includes=true"}), std::string("Header\nBody\n"));
}

TEST_CASE(IncludeSecurityE2E_sibling_relative_include_works) {
    const std::filesystem::path root = security_test_root("sibling-relative");
    write_file(root / "partial.pbt", "Partial\n");
    write_file(root / "nested" / "main.pbt", "{{ include \"../partial.pbt\" }}");

    REQUIRE_EQ(render_file(root / "nested" / "main.pbt", {"allow_includes=true"}, {root}), std::string("Partial\n"));
}

TEST_CASE(IncludeSecurityE2E_traversal_outside_allowed_roots_is_rejected) {
    const std::filesystem::path root = security_test_root("traversal-escape");
    const std::filesystem::path outside = root.parent_path() / (root.filename().string() + "-outside");
    write_file(outside / "secret.txt", "LEAKED\n");
    write_file(root / "nested" / "main.pbt",
               "{{ include \"../../" + (root.filename().string() + "-outside") + "/secret.txt\" }}");

    expect_render_file_error(root / "nested" / "main.pbt", {"allow_includes=true"}, "escapes allowed roots");
}

TEST_CASE(IncludeSecurityE2E_direct_include_cycle_is_rejected) {
    const std::filesystem::path root = security_test_root("direct-cycle");
    write_file(root / "cycle_a.pbt", "{{ include \"cycle_b.pbt\" }}");
    write_file(root / "cycle_b.pbt", "{{ include \"cycle_a.pbt\" }}");

    expect_render_file_error(root / "cycle_a.pbt", {"allow_includes=true"}, "Include cycle detected");
}

TEST_CASE(IncludeSecurityE2E_self_include_cycle_is_rejected) {
    const std::filesystem::path root = security_test_root("self-cycle");
    write_file(root / "main.pbt", "start{{ include \"main.pbt\" }}");

    expect_render_file_error(root / "main.pbt", {"allow_includes=true"}, "Include cycle detected");
}

TEST_CASE(IncludeSecurityE2E_indirect_cycle_inside_conditional_branch_is_rejected) {
    const std::filesystem::path root = security_test_root("conditional-cycle");
    write_file(root / "main.pbt", "{{ if enabled }}{{ include \"partial.pbt\" }}{{ endif }}");
    write_file(root / "partial.pbt", "{{ if enabled }}{{ include \"main.pbt\" }}{{ endif }}");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = root / "main.pbt";
    command.rule_args = {"allow_includes=true"};
    command.define_args = {"enabled=true"};

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(IncludeSecurityE2E_allow_includes_disabled_blocks_every_include) {
    const std::filesystem::path root = security_test_root("includes-disabled");
    write_file(root / "partial.pbt", "Partial\n");
    write_file(root / "main.pbt", "{{ include \"partial.pbt\" }}");

    expect_render_file_error(root / "main.pbt", {"allow_includes=false"});
}

TEST_CASE(IncludeSecurityE2E_max_include_depth_limits_nested_chain) {
    const std::filesystem::path root = security_test_root("max-depth");
    write_file(root / "level2.pbt", "deep");
    write_file(root / "level1.pbt", "{{ include \"./level2.pbt\" }}");
    write_file(root / "main.pbt", "{{ include \"./level1.pbt\" }}");

    expect_render_file_error(root / "main.pbt", {"allow_includes=true", "max_include_depth=1"});
}

TEST_CASE(IncludeSecurityE2E_missing_include_path_is_rejected) {
    const std::filesystem::path root = security_test_root("missing-include");
    write_file(root / "main.pbt", "{{ include \"missing.pbt\" }}");

    expect_render_file_error(root / "main.pbt", {"allow_includes=true"}, "Include not found");
}

TEST_CASE(IncludeSecurityE2E_absolute_include_path_is_rejected) {
    const std::filesystem::path root = security_test_root("absolute-outside");
    write_file(root / "main.pbt", "{{ include \"/etc/passwd\" }}");

    expect_render_file_error(root / "main.pbt", {"allow_includes=true"}, "Absolute include paths are not allowed");
}

TEST_CASE(IncludeSecurityE2E_overlong_include_path_is_rejected) {
    const std::filesystem::path root = security_test_root("overlong-path");
    write_file(root / "main.pbt", "{{ include \"" + std::string(5000, 'a') + "\" }}");

    expect_render_file_error(root / "main.pbt", {"allow_includes=true"}, "Include path is too long");
}
