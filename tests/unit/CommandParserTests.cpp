#include "TestHarness.h"

#include "cli/CommandParser.h"
#include "support/Diagnostic.h"

TEST_CASE(CommandParser_parse_render_options) {
    prebyte::CommandParser parser;
    const prebyte::Command command = parser.parse({"input.txt", "-o", "output.txt", "-I", "shared", "-Dname=Ada", "--benchmark", "-X"});

    REQUIRE(command.input_path.has_value());
    REQUIRE_EQ(command.input_path->string(), std::string("input.txt"));
    REQUIRE(command.output_path.has_value());
    REQUIRE_EQ(command.output_path->string(), std::string("output.txt"));
    REQUIRE_EQ(command.include_paths.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(command.include_paths[0].string(), std::string("shared"));
    REQUIRE_EQ(command.define_args.size(), static_cast<std::size_t>(1));
    REQUIRE(command.benchmark);
    REQUIRE(command.debug);
}

TEST_CASE(CommandParser_parse_list_rules) {
    prebyte::CommandParser parser;
    const prebyte::Command command = parser.parse({"list", "rules"});
    REQUIRE_EQ(static_cast<int>(command.mode), static_cast<int>(prebyte::CommandMode::ListRules));
}

TEST_CASE(CommandParser_parse_list_profiles) {
    prebyte::CommandParser parser;
    const prebyte::Command command = parser.parse({"list", "profiles"});
    REQUIRE_EQ(static_cast<int>(command.mode), static_cast<int>(prebyte::CommandMode::ListProfiles));
}

TEST_CASE(CommandParser_parse_list_ignores_aliases) {
    prebyte::CommandParser parser;
    const prebyte::Command singular = parser.parse({"list", "ignore"});
    const prebyte::Command plural = parser.parse({"list", "ignores"});

    REQUIRE_EQ(static_cast<int>(singular.mode), static_cast<int>(prebyte::CommandMode::ListIgnores));
    REQUIRE_EQ(static_cast<int>(plural.mode), static_cast<int>(prebyte::CommandMode::ListIgnores));
}

TEST_CASE(CommandParser_parse_list_rules_with_options) {
    prebyte::CommandParser parser;
    const prebyte::Command command = parser.parse({"list", "rules", "-s", "settings.yaml", "-I", "shared", "-p", "friendly", "-r", "trim=true"});

    REQUIRE_EQ(static_cast<int>(command.mode), static_cast<int>(prebyte::CommandMode::ListRules));
    REQUIRE(command.settings_path.has_value());
    REQUIRE_EQ(command.settings_path->string(), std::string("settings.yaml"));
    REQUIRE_EQ(command.include_paths.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(command.include_paths[0].string(), std::string("shared"));
    REQUIRE_EQ(command.profile_names.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(command.profile_names[0], std::string("friendly"));
    REQUIRE_EQ(command.rule_args.size(), static_cast<std::size_t>(1));
    REQUIRE_EQ(command.rule_args[0], std::string("trim=true"));
}

TEST_CASE(CommandParser_parse_render_args_after_input) {
    prebyte::CommandParser parser;
    const prebyte::Command command = parser.parse({"input.txt", "first", "second"});

    REQUIRE(command.input_path.has_value());
    REQUIRE_EQ(command.input_path->string(), std::string("input.txt"));
    REQUIRE_EQ(command.render_args.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(command.render_args[0], std::string("first"));
    REQUIRE_EQ(command.render_args[1], std::string("second"));
}

TEST_CASE(CommandParser_parse_render_args_for_stdin_after_terminator) {
    prebyte::CommandParser parser;
    const prebyte::Command command = parser.parse({"--", "first", "second"});

    REQUIRE(!command.input_path.has_value());
    REQUIRE_EQ(command.render_args.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(command.render_args[0], std::string("first"));
    REQUIRE_EQ(command.render_args[1], std::string("second"));
}

TEST_CASE(CommandParser_reject_unknown_argument) {
    prebyte::CommandParser parser;
    REQUIRE_THROWS_AS(parser.parse({"--wat"}), prebyte::DiagnosticError);
}
