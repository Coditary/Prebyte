#include "TestHarness.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>

#include "Engine.h"
#include "PrebyteEngine.h"
#include "app/AppRunner.h"
#include "app/Command.h"
#include "io/InputBuffer.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "support/Diagnostic.h"

namespace {

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path test_temp_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-tests" / name;
    std::error_code error;
    if (std::filesystem::exists(root, error)) {
        std::filesystem::permissions(root,
                                     std::filesystem::perms::owner_all | std::filesystem::perms::group_all
                                         | std::filesystem::perms::others_all,
                                     std::filesystem::perm_options::add,
                                     error);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied, error)) {
            std::filesystem::permissions(entry.path(),
                                         std::filesystem::perms::owner_all | std::filesystem::perms::group_all
                                             | std::filesystem::perms::others_all,
                                         std::filesystem::perm_options::add,
                                         error);
        }
    }
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

}

TEST_CASE(AppRunner_render_simple_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/render_simple/input.txt";
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Hello Ada\n"));
}

TEST_CASE(AppRunner_rules_variable_delimiters_replace_tabs_and_tab_size_affect_render) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "\t<< name >>";
    command.define_args = {"name=Ada"};
    command.rule_args = {
        "variable_prefix=<<",
        "variable_suffix=>>",
        "replace_tabs=true",
        "tab_size=2",
    };

    prebyte::AppRunner runner;
    REQUIRE_EQ(runner.execute(command), std::string("  Ada"));
}

TEST_CASE(AppRunner_legacy_include_path_rule_still_resolves_includes) {
    const std::filesystem::path root = test_temp_root("legacy-include-path-rule");
    write_file(root / "shared" / "java.pbt", "Legacy OK");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ include \"java\" }}";
    command.rule_args = {"include_path=" + (root / "shared").string()};

    prebyte::AppRunner runner;
    REQUIRE_EQ(runner.execute(command), std::string("Legacy OK"));
}

TEST_CASE(AppRunner_output_encoding_and_error_on_false_input_rules_apply_to_file_output_and_conditions) {
    const std::filesystem::path root = test_temp_root("apprunner-rule-behavior");
    const std::filesystem::path output_path = root / "out.txt";

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ if false }}bad{{ else }}ok{{ endif }}";
    command.rule_args = {"output_encoding=utf-16", "error_on_false_input=true"};

    prebyte::AppRunner runner;
    try {
        static_cast<void>(runner.execute(command));
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError& error) {
        REQUIRE_EQ(error.diagnostic().message, std::string("False input is not allowed in condition"));
    }

    prebyte::Command write_command;
    write_command.mode = prebyte::CommandMode::Render;
    write_command.inline_input = "Hello";
    write_command.output_path = output_path;
    write_command.rule_args = {"output_encoding=utf-16", "error_on_false_input=false"};
    runner.run(write_command);

    const std::string bytes = std::string(prebyte::InputBuffer::from_file(output_path).view());
    REQUIRE_EQ(bytes.size(), static_cast<std::size_t>(12));
    REQUIRE_EQ(static_cast<unsigned char>(bytes[0]), 0xFFu);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[1]), 0xFEu);
}

TEST_CASE(AppRunner_render_include_and_if_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/render_include_if/input.txt";
    command.define_args = {"name=Ada", "enabled=true"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Header for Ada\n\nEnabled\nFooter\n"));
}

TEST_CASE(Engine_compile_file_renders_fixture_with_relative_include) {
    prebyte::Engine engine;
    const std::filesystem::path path = "tests/fixtures/render_include_if/input.txt";
    const prebyte::CompiledTemplate tpl = engine.compile_file(path);

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");
    ctx.set("enabled", "true");

    const std::string output = engine.render(tpl, ctx);
    REQUIRE_EQ(output, std::string("Header for Ada\n\nEnabled\nFooter\n"));
}

TEST_CASE(Engine_render_to_streams_fixture_with_relative_include) {
    prebyte::Engine engine;
    const std::filesystem::path path = "tests/fixtures/render_include_if/input.txt";
    const prebyte::CompiledTemplate tpl = engine.compile_file(path);

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");
    ctx.set("enabled", "true");

    std::string output;
    std::size_t chunk_count = 0;
    engine.render_to(tpl, [&](std::string_view chunk) {
        ++chunk_count;
        output.append(chunk.data(), chunk.size());
    }, ctx);

    REQUIRE_EQ(output, std::string("Header for Ada\n\nEnabled\nFooter\n"));
    REQUIRE(chunk_count > 1);
}

TEST_CASE(Engine_render_to_streams_mixed_control_flow_from_compiled_file) {
    const std::filesystem::path root = test_temp_root("engine-stream-mixed-control-flow");
    write_file(root / "partial.pbt", "{{ if member.admin }}*{{ else }}-{{ endif }}{{ member.name }}");
    write_file(root / "main.pbt",
               "{{ if members }}{{ for member in members }}{{ include \"./partial\" }};{{ endfor }}{{ elseif archived }}archived{{ else }}empty{{ endif }}");

    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile_file(root / "main.pbt");

    prebyte::RenderContext ctx;
    ctx.set("archived", "false");
    prebyte::Data::Array members;
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Ada")}, {"admin", prebyte::Data(true)}}));
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Grace")}, {"admin", prebyte::Data(false)}}));
    ctx.set("members", prebyte::Value::list(std::move(members)));

    std::string output;
    std::size_t chunk_count = 0;
    engine.render_to(tpl, [&](std::string_view chunk) {
        ++chunk_count;
        output.append(chunk.data(), chunk.size());
    }, ctx);

    REQUIRE_EQ(output, std::string("*Ada;-Grace;"));
    REQUIRE(chunk_count > 1);
}

TEST_CASE(AppRunner_render_settings_profile_merge_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/settings_profile_merge/input.txt";
    command.settings_path = "tests/fixtures/settings_profile_merge/settings.yaml";
    command.profile_names = {"friendly"};
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Hi Ada\n"));
}

TEST_CASE(AppRunner_apply_file_rule_to_include_extension) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/file_rule_include/main.txt";
    command.rule_args = {".md::default_variable_value=Fallback"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Start\nValue=Fallback\n\nEnd\n"));
}

TEST_CASE(AppRunner_strict_variable_failure) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/render_simple/input.txt";
    command.rule_args = {"strict_variables=true"};

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_render_lua_inline_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/lua_inline/input.txt";
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("ADA\n"));
}

TEST_CASE(AppRunner_render_lua_block_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/lua_block/input.txt";
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Hello Ada\n"));
}

TEST_CASE(AppRunner_render_for_else_fixture) {
    const std::filesystem::path root = test_temp_root("for-else-loop");
    const std::filesystem::path source_path = root / "sample.pbt";
    write_file(source_path, "{{ for item in missing }}{{ item }}{{ else }}empty{{ endfor }}\n");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = source_path;

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("empty\n"));
}

TEST_CASE(AppRunner_render_mixed_native_lua_condition_fixture) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = "tests/fixtures/lua_condition/input.txt";
    command.define_args = {"enabled=true", "name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("ok\n"));
}

TEST_CASE(AppRunner_native_false_string_is_false) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ if disabled }}bad{{ else }}ok{{ endif }}";
    command.define_args = {"disabled=false"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("ok"));
}

TEST_CASE(AppRunner_benchmark_output_includes_lua_cache_stats) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ lua \"counter = (counter or 0) + 1; return counter\" }} {{ lua \"counter = (counter or 0) + 1; return counter\" }}";
    command.benchmark = true;

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("1 1\n[benchmark] ") == 0);
    REQUIRE(output.find("lua_cache_hits=1") != std::string::npos);
    REQUIRE(output.find("lua_cache_misses=1") != std::string::npos);
}

TEST_CASE(AppRunner_render_if_lua_block_condition) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ if lua:block }}\nlocal allowed = enabled == 'true'\nreturn allowed\n{{ endlua }}ok{{ else }}no{{ endif }}";
    command.define_args = {"enabled=true"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("ok"));
}

TEST_CASE(AppRunner_render_elseif_lua_block_condition) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ if disabled == true }}no{{ elseif lua:block }}\nlocal allowed = enabled == 'true'\nreturn allowed\n{{ endlua }}ok{{ else }}fallback{{ endif }}";
    command.define_args = {"disabled=false", "enabled=true"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("ok"));
}

TEST_CASE(AppRunner_render_lua_respects_configured_memory_limit) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ lua \"return string.rep('x', 2097152)\" }}";
    command.rule_args = {"lua_memory_limit_bytes=1048576"};

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_render_lua_respects_configured_instruction_limit) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ lua \"local sum = 0 for i = 1, 1000 do sum = sum + i end return sum\" }}";
    command.rule_args = {"lua_instruction_limit=10"};

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_explain_lua_includes_helpers_and_limits) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Explain;
    command.explain_topic = "lua";

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("upper(value)") != std::string::npos);
    REQUIRE(output.find("starts_with(value, prefix)") != std::string::npos);
    REQUIRE(output.find("lua_instruction_limit=100000") != std::string::npos);
    REQUIRE(output.find("lua_memory_limit_bytes=4194304") != std::string::npos);
}

TEST_CASE(AppRunner_help_includes_lua_helpers_and_limits) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Help;

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("Lua helpers:") != std::string::npos);
    REQUIRE(output.find("ends_with(value, suffix)") != std::string::npos);
    REQUIRE(output.find("lua_instruction_limit=100000") != std::string::npos);
}

TEST_CASE(AppRunner_list_rules_applies_settings_profiles_and_cli_overrides) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::ListRules;
    command.settings_path = "tests/fixtures/settings_profile_merge/settings.yaml";
    command.profile_names = {"friendly"};
    command.rule_args = {"trim=true", ".md::default_variable_value=Fallback"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("trim=true") != std::string::npos);
    REQUIRE(output.find("extension:.md::default_variable_value=Fallback") != std::string::npos);
}

TEST_CASE(AppRunner_list_vars_applies_settings_profiles_and_cli_defines) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::ListVars;
    command.settings_path = "tests/fixtures/settings_profile_merge/settings.yaml";
    command.profile_names = {"friendly"};
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("greeting=Hi") != std::string::npos);
    REQUIRE(output.find("name=Ada") != std::string::npos);
}

TEST_CASE(AppRunner_render_named_structured_imports) {
    const std::filesystem::path root = test_temp_root("named-structured-imports");
    const std::filesystem::path template_path = root / "input.pbt";
    const std::filesystem::path user_path = root / "user.json";
    const std::filesystem::path items_path = root / "items.yaml";
    const std::filesystem::path config_path = root / "config.toml";

    write_file(template_path,
               "{{ user.name }}\n{{ items[1] }}\n{{ config.server.host }}:{{ config.server.port }}\n");
    write_file(user_path, "{\"name\":\"Ada\"}");
    write_file(items_path, "- Ada\n- Grace\n");
    write_file(config_path, "[server]\nhost=\"localhost\"\nport=8080\n");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = template_path;
    command.define_args = {
        "user=@" + user_path.string(),
        "items=@" + items_path.string(),
        "config=@" + config_path.string(),
    };

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Ada\nGrace\nlocalhost:8080\n"));
}

TEST_CASE(AppRunner_list_vars_shows_named_structured_import_placeholders) {
    const std::filesystem::path root = test_temp_root("list-vars-structured-import");
    const std::filesystem::path user_path = root / "user.json";
    const std::filesystem::path items_path = root / "items.yaml";
    write_file(user_path, "{\"name\":\"Ada\"}");
    write_file(items_path, "- Ada\n- Grace\n");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::ListVars;
    command.define_args = {
        "user=@" + user_path.string(),
        "items=@" + items_path.string(),
    };

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE(output.find("user={...}") != std::string::npos);
    REQUIRE(output.find("items=[...]") != std::string::npos);
}

TEST_CASE(AppRunner_list_profiles_shows_available_profile_names) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::ListProfiles;
    command.settings_path = "tests/fixtures/settings_profile_merge/settings.yaml";

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("friendly") != std::string::npos);
    REQUIRE(output.find("strict") != std::string::npos);
}

TEST_CASE(AppRunner_list_ignores_merges_settings_profiles_and_cli) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::ListIgnores;
    command.settings_path = "tests/fixtures/settings_profile_merge/settings.yaml";
    command.profile_names = {"friendly"};
    command.ignore_names = {"cli_only"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("base_ignore") != std::string::npos);
    REQUIRE(output.find("friendly_ignore") != std::string::npos);
    REQUIRE(output.find("cli_only") != std::string::npos);
}

TEST_CASE(AppRunner_render_args_are_available_in_native_and_lua) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ ARGS[0] }} {{ lua \"return ARGS[2]\" }}";
    command.render_args = {"alpha", "beta", "gamma"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE_EQ(output, std::string("alpha gamma"));
}

TEST_CASE(AppRunner_explain_args_describes_render_args) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Explain;
    command.explain_topic = "ARGS";

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("ARGS[index]") != std::string::npos);
    REQUIRE(output.find("prebyte template.txt foo bar") != std::string::npos);
}

TEST_CASE(AppRunner_help_mentions_list_profiles_and_render_args) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Help;

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("list rules|vars|profiles|ignore|ignores") != std::string::npos);
    REQUIRE(output.find("ARGS[index]") != std::string::npos);
    REQUIRE(output.find("max_include_depth") != std::string::npos);
    REQUIRE(output.find("forbidden_env_vars") != std::string::npos);
    REQUIRE(output.find("error_on_false_input") != std::string::npos);
    REQUIRE(output.find("utf-16") != std::string::npos);
}

TEST_CASE(AppRunner_explain_rules_mentions_new_runtime_limits) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Explain;
    command.explain_topic = "rules";

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("forbidden_env_vars") != std::string::npos);
    REQUIRE(output.find("max_include_depth") != std::string::npos);
    REQUIRE(output.find("max_render_time_ms") != std::string::npos);
    REQUIRE(output.find("max_output_size_bytes") != std::string::npos);
    REQUIRE(output.find("max_loop_iteration") != std::string::npos);
    REQUIRE(output.find("output_encoding") != std::string::npos);
    REQUIRE(output.find("error_on_false_input") != std::string::npos);
}

TEST_CASE(AppRunner_list_rules_shows_new_rule_values) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::ListRules;
    command.rule_args = {
        "forbidden_env_vars=HOME,SECRET",
        "max_include_depth=2",
        "max_render_time_ms=50",
        "max_output_size_bytes=128",
        "max_loop_iteration=3",
    };

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("forbidden_env_vars=HOME,SECRET") != std::string::npos);
    REQUIRE(output.find("max_include_depth=2") != std::string::npos);
    REQUIRE(output.find("max_render_time_ms=50") != std::string::npos);
    REQUIRE(output.find("max_output_size_bytes=128") != std::string::npos);
    REQUIRE(output.find("max_loop_iteration=3") != std::string::npos);
}

TEST_CASE(AppRunner_forbidden_env_vars_override_allow_env) {
    ::setenv("PREBYTE_APP_ALLOWED_ENV", "Ada", 1);
    ::setenv("PREBYTE_APP_BLOCKED_ENV", "Secret", 1);

    prebyte::Command allowed;
    allowed.mode = prebyte::CommandMode::Render;
    allowed.inline_input = "{{ PREBYTE_APP_ALLOWED_ENV }}";
    allowed.rule_args = {"allow_env=true", "forbidden_env_vars=PREBYTE_APP_BLOCKED_ENV"};

    prebyte::AppRunner runner;
    REQUIRE_EQ(runner.execute(allowed), std::string("Ada"));

    prebyte::Command blocked = allowed;
    blocked.inline_input = "{{ PREBYTE_APP_BLOCKED_ENV }}";
    REQUIRE_THROWS_AS(runner.execute(blocked), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_max_include_depth_and_allow_includes_interaction) {
    const std::filesystem::path root = test_temp_root("app-runner-max-include-depth");
    write_file(root / "level2.pbt", "deep");
    write_file(root / "level1.pbt", "{{ include \"./level2\" }}");
    write_file(root / "main.pbt", "{{ include \"./level1\" }}");

    prebyte::AppRunner runner;

    prebyte::Command depth_fail;
    depth_fail.mode = prebyte::CommandMode::Render;
    depth_fail.input_path = root / "main.pbt";
    depth_fail.rule_args = {"max_include_depth=1"};
    REQUIRE_THROWS_AS(runner.execute(depth_fail), prebyte::DiagnosticError);

    prebyte::Command includes_disabled = depth_fail;
    includes_disabled.rule_args = {"allow_includes=false", "max_include_depth=1"};
    REQUIRE_THROWS_AS(runner.execute(includes_disabled), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_profile_override_applies_new_rules) {
    const std::filesystem::path root = test_temp_root("profile-override-new-rules");
    write_file(root / "partial.pbt", "x");
    write_file(root / "main.pbt", "{{ include \"./partial\" }}");
    write_file(root / "settings.json", std::string("{\n")
        + "  \"profiles\": {\n"
        + "    \"strict\": {\n"
        + "      \"rules\": {\n"
        + "        \"max_include_depth\": \"0\"\n"
        + "      }\n"
        + "    }\n"
        + "  }\n"
        + "}\n");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = root / "main.pbt";
    command.settings_path = root / "settings.json";
    command.profile_names = {"strict"};

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_file_rule_override_applies_new_rules_to_included_file) {
    const std::filesystem::path root = test_temp_root("file-rule-new-rules");
    write_file(root / "partial.pbt", "0123456789");
    write_file(root / "main.pbt", "{{ include \"./partial\" }}");
    write_file(root / "settings.json", std::string("{\n")
        + "  \"file_rules\": {\n"
        + "    \"partial\": {\n"
        + "      \"max_output_size_bytes\": \"5\"\n"
        + "    }\n"
        + "  }\n"
        + "}\n");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = root / "main.pbt";
    command.settings_path = root / "settings.json";

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_file_rule_override_applies_forbidden_env_vars_to_included_file) {
    const std::filesystem::path root = test_temp_root("file-rule-forbidden-env");
    ::setenv("PREBYTE_FILE_RULE_BLOCKED", "secret", 1);
    write_file(root / "partial.pbt", "{{ PREBYTE_FILE_RULE_BLOCKED }}");
    write_file(root / "main.pbt", "{{ include \"./partial\" }}");
    write_file(root / "settings.json", std::string("{\n")
        + "  \"rules\": {\n"
        + "    \"allow_env\": \"true\"\n"
        + "  },\n"
        + "  \"file_rules\": {\n"
        + "    \"partial\": {\n"
        + "      \"forbidden_env_vars\": \"PREBYTE_FILE_RULE_BLOCKED\"\n"
        + "    }\n"
        + "  }\n"
        + "}\n");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = root / "main.pbt";
    command.settings_path = root / "settings.json";

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_max_output_size_bytes_rule_fails_early) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "Hello!";
    command.rule_args = {"max_output_size_bytes=5"};

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(AppRunner_max_loop_iteration_rule_applies_per_nested_loop) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ fn groups() lua:block }}return { {'A', 'B'}, {'C'} }{{ endfn }}{{ for group in groups() }}{{ for item in group }}{{ item }}{{ endfor }}|{{ endfor }}";
    command.rule_args = {"max_loop_iteration=1"};

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);

    command.rule_args = {"max_loop_iteration=2"};
    REQUIRE_EQ(runner.execute(command), std::string("AB|C|"));
}

TEST_CASE(AppRunner_max_render_time_rule_interrupts_lua_block) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ lua:block }} while true do end return 'x' {{ endlua }}";
    command.rule_args = {"max_render_time_ms=0"};

    prebyte::AppRunner runner;
    REQUIRE_THROWS_AS(runner.execute(command), prebyte::DiagnosticError);
}

TEST_CASE(PrebyteEngine_render_args_are_available) {
    prebyte::Prebyte engine;
    engine.add_argument("alpha");
    engine.add_argument("beta");

    const std::string output = engine.process("{{ ARGS[1] }}");
    REQUIRE_EQ(output, std::string("beta"));
}

TEST_CASE(PrebyteEngine_process_file_renders_include_loop_template) {
    const std::filesystem::path root = test_temp_root("engine-process-file-loop-include");
    write_file(root / "partial.pbt", "<{{ loop.index }}:{{ item }}>");
    write_file(root / "main.pbt", "A{{ for item in items }}{{ include \"./partial\" }}{{ endfor }}Z");
    write_file(root / "items.yaml", "- Ada\n- Grace\n");

    prebyte::Prebyte engine;
    engine.set_variable("items", "@" + (root / "items.yaml").string());

    const std::string output = engine.process_file((root / "main.pbt").string());
    REQUIRE_EQ(output, std::string("A<1:Ada><2:Grace>Z"));
}

TEST_CASE(AppRunner_include_uses_cli_include_path_first_match) {
    const std::filesystem::path root = test_temp_root("include-cli-path");
    write_file(root / "a" / "shared" / "java.pbt", "A");
    write_file(root / "b" / "shared" / "java.pbt", "B");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ include \"java\" }}";
    command.include_paths = {root / "a/shared", root / "b/shared"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("A"));
}

TEST_CASE(AppRunner_include_uses_directory_index_template) {
    const std::filesystem::path root = test_temp_root("include-index");
    write_file(root / "shared" / "maven" / "java" / "index.pbt", "Index OK");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ include \"maven/java\" }}";
    command.include_paths = {root / "shared"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Index OK"));
}

TEST_CASE(AppRunner_render_top_level_pbc_file) {
    const std::filesystem::path root = test_temp_root("top-level-pbc");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::filesystem::path logical_path = root / "sample";
    write_file(source_path, "Hello {{ name }}");

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source("Hello {{ name }}", source_path, logical_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(compiled_path, serializer.serialize(program));

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = compiled_path;
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Hello Ada"));
}

TEST_CASE(Engine_load_compiled_file_renders_top_level_pbc) {
    const std::filesystem::path root = test_temp_root("engine-top-level-pbc");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::filesystem::path logical_path = root / "sample";
    write_file(source_path, "Hello {{ name }}");

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source("Hello {{ name }}", source_path, logical_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(compiled_path, serializer.serialize(program));

    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.load_compiled_file(compiled_path);

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");

    REQUIRE_EQ(tpl.source_path(), source_path);
    REQUIRE_EQ(tpl.logical_path(), logical_path);
    REQUIRE_EQ(engine.render(tpl, ctx), std::string("Hello Ada"));
}

TEST_CASE(Engine_load_compiled_file_render_to_streams_nested_control_flow) {
    const std::filesystem::path root = test_temp_root("engine-top-level-pbc-nested-control-flow");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::filesystem::path logical_path = root / "sample";
    const std::string source =
        "{{ if members }}{{ for member in members }}{{ include \"./partial\" }};{{ endfor }}{{ elseif archived }}archived{{ else }}empty{{ endif }}";
    write_file(root / "partial.pbt", "{{ if member.admin }}*{{ else }}-{{ endif }}{{ member.name }}");
    write_file(source_path, source);

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source(source, source_path, logical_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(compiled_path, serializer.serialize(program));

    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.load_compiled_file(compiled_path);

    prebyte::RenderContext ctx;
    ctx.set("archived", "false");
    prebyte::Data::Array members;
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Ada")}, {"admin", prebyte::Data(true)}}));
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Grace")}, {"admin", prebyte::Data(false)}}));
    ctx.set("members", prebyte::Value::list(std::move(members)));

    std::string output;
    std::size_t chunk_count = 0;
    engine.render_to(tpl, [&](std::string_view chunk) {
        ++chunk_count;
        output.append(chunk.data(), chunk.size());
    }, ctx);

    REQUIRE_EQ(output, std::string("*Ada;-Grace;"));
    REQUIRE(chunk_count > 1);
}

TEST_CASE(AppRunner_render_top_level_pbc_file_with_named_structured_imports) {
    const std::filesystem::path root = test_temp_root("top-level-pbc-structured-imports");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::filesystem::path logical_path = root / "sample";
    const std::filesystem::path user_path = root / "user.json";
    const std::filesystem::path items_path = root / "items.yaml";
    write_file(source_path, "{{ user.name }} {{ items[1] }}");
    write_file(user_path, "{\"name\":\"Ada\"}");
    write_file(items_path, "- Ada\n- Grace\n");

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source("{{ user.name }} {{ items[1] }}", source_path, logical_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(compiled_path, serializer.serialize(program));

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = compiled_path;
    command.define_args = {"user=@" + user_path.string(), "items=@" + items_path.string()};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Ada Grace"));
}

TEST_CASE(AppRunner_render_source_file_from_adjacent_pbc_without_reading_source) {
    const std::filesystem::path root = test_temp_root("top-level-adjacent-pbc");
    const std::filesystem::path source_path = root / "sample.txt";
    write_file(source_path, "Hello {{ name }}");

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source("Hello {{ name }}", source_path, source_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(serializer.compiled_path_for_source(source_path), serializer.serialize(program));

    std::error_code permissions_error;
    std::filesystem::permissions(source_path,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::group_read
                                     | std::filesystem::perms::others_read,
                                 std::filesystem::perm_options::remove,
                                 permissions_error);
    REQUIRE(!permissions_error);

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = source_path;
    command.define_args = {"name=Ada"};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Hello Ada"));
}

TEST_CASE(AppRunner_render_source_file_from_adjacent_pbc_with_named_structured_imports) {
    const std::filesystem::path root = test_temp_root("adjacent-pbc-structured-imports");
    const std::filesystem::path source_path = root / "sample.txt";
    const std::filesystem::path user_path = root / "user.json";
    const std::filesystem::path config_path = root / "config.toml";
    write_file(source_path, "{{ user.name }} {{ config.server.host }}");
    write_file(user_path, "{\"name\":\"Ada\"}");
    write_file(config_path, "[server]\nhost=\"localhost\"\n");

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source("{{ user.name }} {{ config.server.host }}", source_path, source_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(serializer.compiled_path_for_source(source_path), serializer.serialize(program));

    std::error_code permissions_error;
    std::filesystem::permissions(source_path,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::group_read
                                     | std::filesystem::perms::others_read,
                                 std::filesystem::perm_options::remove,
                                 permissions_error);
    REQUIRE(!permissions_error);

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = source_path;
    command.define_args = {"user=@" + user_path.string(), "config=@" + config_path.string()};

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Ada localhost"));
}

TEST_CASE(AppRunner_reuse_in_memory_compiled_template_when_pbc_write_fails) {
    const std::filesystem::path root = test_temp_root("in-memory-compiled-cache");
    const std::filesystem::path source_path = root / "sample.txt";
    write_file(source_path, "Hello {{ name }}");

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = source_path;
    command.define_args = {"name=Ada"};

    std::error_code permissions_error;
    std::filesystem::permissions(root,
                                 std::filesystem::perms::owner_write | std::filesystem::perms::group_write
                                     | std::filesystem::perms::others_write,
                                 std::filesystem::perm_options::remove,
                                 permissions_error);
    REQUIRE(!permissions_error);

    prebyte::AppRunner runner;
    REQUIRE_EQ(runner.execute(command), std::string("Hello Ada"));

    std::filesystem::permissions(source_path,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::group_read
                                     | std::filesystem::perms::others_read,
                                 std::filesystem::perm_options::remove,
                                 permissions_error);
    REQUIRE(!permissions_error);

    REQUIRE_EQ(runner.execute(command), std::string("Hello Ada"));

    std::filesystem::permissions(source_path,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_write
                                     | std::filesystem::perms::group_read | std::filesystem::perms::group_write
                                     | std::filesystem::perms::others_read | std::filesystem::perms::others_write,
                                 std::filesystem::perm_options::add,
                                 permissions_error);
    REQUIRE(!permissions_error);
    std::filesystem::permissions(root,
                                 std::filesystem::perms::owner_write | std::filesystem::perms::group_write
                                     | std::filesystem::perms::others_write,
                                 std::filesystem::perm_options::add,
                                 permissions_error);
    REQUIRE(!permissions_error);
}

TEST_CASE(AppRunner_include_uses_default_shared_root) {
    const std::filesystem::path root = test_temp_root("default-shared-root");
    const std::filesystem::path shared = root / ".local/share/prebyte/java/index.pbt";
    write_file(shared, "Shared OK");
    setenv("HOME", root.c_str(), 1);

    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input = "{{ include \"java\" }}";

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);
    REQUIRE_EQ(output, std::string("Shared OK"));
}

TEST_CASE(AppRunner_render_functions_and_builtins) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.inline_input =
        "{{ fn greet(name) }}Hello {{ name }} from {{ __OS__ }}{{ endfn }}"
        "{{ greet(\"Ada\") }}"
        "|{{ fn users() lua:block }}return { { name = 'Grace' }, { name = 'Linus' } }{{ endfn }}"
        "{{ for user in users() }}{{ user.name }};{{ endfor }}"
        "|{{ __UUID__ == __UUID__ }}|{{ __RANDOM__ == __RANDOM__ }}";

    prebyte::AppRunner runner;
    const std::string output = runner.execute(command);

    REQUIRE(output.find("Hello Ada from ") == 0);
    REQUIRE(output.find("|Grace;Linus;|true|true") != std::string::npos);
}
