#include "TestHarness.h"

#include "datatypes/Data.h"
#include "runtime/lua/LuaRuntime.h"
#include "support/Diagnostic.h"
#include "support/SourceSpan.h"

#include <chrono>
#include <limits>
#include <string>

namespace {

prebyte::SourceSpan make_span(const char* file_path = "runtime.txt", std::size_t line = 5) {
    prebyte::SourceSpan span;
    span.file_path = file_path;
    span.start.line = line;
    return span;
}

prebyte::Value execute_lua(prebyte::LuaRuntime& runtime,
                           const std::string& source,
                           prebyte::RenderSession& session,
                           const prebyte::EffectiveSettings& settings,
                           prebyte::LuaChunkMode mode = prebyte::LuaChunkMode::Predicate,
                           const std::filesystem::path& current_file = "runtime.txt",
                           const prebyte::SourceSpan& span = make_span()) {
    return runtime.execute(source, mode, settings, session, current_file, span);
}

void expect_lua_diagnostic(const auto& callable,
                           const std::string& expected_message_substring,
                           const std::string& expected_code = "LUA001") {
    try {
        callable();
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError& error) {
        REQUIRE_EQ(error.diagnostic().code, expected_code);
        REQUIRE(error.diagnostic().message.find(expected_message_substring) != std::string::npos);
    }
}

}

TEST_CASE(LuaRuntime_execute_returns_scalars_and_nil) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    REQUIRE_EQ(execute_lua(runtime, "return 42", session, settings).to_string(), std::string("42"));
    REQUIRE_EQ(execute_lua(runtime, "return 'hello'", session, settings).to_string(), std::string("hello"));
    REQUIRE(execute_lua(runtime, "return true", session, settings).to_bool());
    REQUIRE(!execute_lua(runtime, "return false", session, settings).to_bool());
    REQUIRE(execute_lua(runtime, "return nil", session, settings).is_null());
}

TEST_CASE(LuaRuntime_execute_reads_session_variables_args_and_vars) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");
    session.variables.set_value("count", prebyte::Value(3.0));
    session.args = {"alpha", "beta"};

    prebyte::EffectiveSettings settings;

    const prebyte::Value value = execute_lua(
        runtime,
        "return name .. '|' .. tostring(count) .. '|' .. vars.name .. '|' .. ARGS[0] .. '|' .. ARGS[1]",
        session,
        settings);

    REQUIRE_EQ(value.to_string(), std::string("Ada|3.0|Ada|alpha|beta"));
}

TEST_CASE(LuaRuntime_execute_exposes_registered_helpers) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    session.variables.set("name", "  Ada Lovelace  ");

    prebyte::EffectiveSettings settings;

    REQUIRE_EQ(execute_lua(runtime, "return upper(name)", session, settings).to_string(), std::string("  ADA LOVELACE  "));
    REQUIRE_EQ(execute_lua(runtime, "return lower(upper(name))", session, settings).to_string(), std::string("  ada lovelace  "));
    REQUIRE_EQ(execute_lua(runtime, "return trim(name)", session, settings).to_string(), std::string("Ada Lovelace"));
    REQUIRE(execute_lua(runtime, "return starts_with(name, '  Ada')", session, settings).to_bool());
    REQUIRE(execute_lua(runtime, "return ends_with(name, 'lace  ')", session, settings).to_bool());
    REQUIRE_EQ(execute_lua(runtime, "return upper(123)", session, settings).to_string(), std::string("123"));
}

TEST_CASE(LuaRuntime_execute_exposes_builtins_and_strict_variables) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;
    settings.strict_variables = true;

    const prebyte::Value value = execute_lua(runtime,
                                             "return __FILE__ .. '|' .. __LINE__ .. '|' .. tostring(strict_variables)",
                                             session,
                                             settings,
                                             prebyte::LuaChunkMode::Predicate,
                                             "runtime.txt",
                                             make_span("runtime.txt", 12));

    REQUIRE_EQ(value.to_string(), std::string("runtime.txt|12|true"));
}

TEST_CASE(LuaRuntime_execute_returns_structured_values) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    const prebyte::Value value = execute_lua(
        runtime,
        R"(return {
            tags = { "admin", "editor" },
            user = { name = "Ada", active = true },
            total = 2
        })",
        session,
        settings);

    REQUIRE(value.is_object());
    REQUIRE_EQ(value.member("total")->to_string(), std::string("2"));
    REQUIRE_EQ(value.member("user")->member("name")->to_string(), std::string("Ada"));
    REQUIRE(value.member("user")->member("active")->to_bool());
    REQUIRE_EQ(value.member("tags")->length(), static_cast<std::size_t>(2));
}

TEST_CASE(LuaRuntime_execute_supports_all_chunk_modes) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    const std::string source = "return 'mode-ok'";

    REQUIRE_EQ(execute_lua(runtime, source, session, settings, prebyte::LuaChunkMode::InlineValue).to_string(),
               std::string("mode-ok"));
    REQUIRE_EQ(execute_lua(runtime, source, session, settings, prebyte::LuaChunkMode::Predicate).to_string(),
               std::string("mode-ok"));
    REQUIRE_EQ(execute_lua(runtime, source, session, settings, prebyte::LuaChunkMode::BlockValue).to_string(),
               std::string("mode-ok"));
}

TEST_CASE(LuaRuntime_chunk_cache_counts_hits_and_misses) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    const std::string source = "return 7";

    REQUIRE_EQ(session.lua_cache_hits, static_cast<std::size_t>(0));
    REQUIRE_EQ(session.lua_cache_misses, static_cast<std::size_t>(0));

    REQUIRE_EQ(execute_lua(runtime, source, session, settings).to_string(), std::string("7"));
    REQUIRE_EQ(session.lua_cache_hits, static_cast<std::size_t>(0));
    REQUIRE_EQ(session.lua_cache_misses, static_cast<std::size_t>(1));

    REQUIRE_EQ(execute_lua(runtime, source, session, settings).to_string(), std::string("7"));
    REQUIRE_EQ(session.lua_cache_hits, static_cast<std::size_t>(1));
    REQUIRE_EQ(session.lua_cache_misses, static_cast<std::size_t>(1));

    REQUIRE_EQ(execute_lua(runtime, "return 8", session, settings).to_string(), std::string("8"));
    REQUIRE_EQ(session.lua_cache_hits, static_cast<std::size_t>(1));
    REQUIRE_EQ(session.lua_cache_misses, static_cast<std::size_t>(2));

    REQUIRE_EQ(execute_lua(runtime, source, session, settings, prebyte::LuaChunkMode::BlockValue).to_string(),
               std::string("7"));
    REQUIRE_EQ(session.lua_cache_hits, static_cast<std::size_t>(1));
    REQUIRE_EQ(session.lua_cache_misses, static_cast<std::size_t>(3));
}

TEST_CASE(LuaRuntime_execute_isolates_lua_globals_between_runs) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    const std::string source = "counter = (counter or 0) + 1; return counter";

    REQUIRE_EQ(execute_lua(runtime, source, session, settings).to_string(), std::string("1"));
    REQUIRE_EQ(execute_lua(runtime, source, session, settings).to_string(), std::string("1"));
}

TEST_CASE(LuaRuntime_execute_recovers_after_runtime_error) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    expect_lua_diagnostic([&]() { static_cast<void>(execute_lua(runtime, "error('boom')", session, settings)); },
                          "boom");

    REQUIRE_EQ(execute_lua(runtime, "return 'recovered'", session, settings).to_string(), std::string("recovered"));
}

TEST_CASE(LuaRuntime_execute_rejects_invalid_lua_syntax) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    expect_lua_diagnostic([&]() { static_cast<void>(execute_lua(runtime, "return )", session, settings)); }, ")");
}

TEST_CASE(LuaRuntime_execute_rejects_missing_variable_access) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    expect_lua_diagnostic([&]() { static_cast<void>(execute_lua(runtime, "return missing_field.name", session, settings)); },
                          "missing_field");
}

TEST_CASE(LuaRuntime_execute_rejects_bad_helper_arity) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    expect_lua_diagnostic([&]() { static_cast<void>(execute_lua(runtime, "return starts_with('Ada')", session, settings)); },
                          "bad argument #2");
}

TEST_CASE(LuaRuntime_execute_rejects_bad_helper_argument_types) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    expect_lua_diagnostic([&]() { static_cast<void>(execute_lua(runtime, "return upper({})", session, settings)); },
                          "bad argument #1");
}

TEST_CASE(LuaRuntime_execute_enforces_instruction_limit) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;
    settings.lua_instruction_limit = 10;
    settings.max_render_time_ms = std::numeric_limits<std::size_t>::max();

    expect_lua_diagnostic(
        [&]() {
            static_cast<void>(execute_lua(runtime,
                                          "local sum = 0 for i = 1, 1000 do sum = sum + i end return sum",
                                          session,
                                          settings));
        },
        "Lua instruction limit exceeded");

    REQUIRE_EQ(execute_lua(runtime, "return 1", session, settings).to_string(), std::string("1"));
}

TEST_CASE(LuaRuntime_execute_enforces_memory_limit) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;
    settings.lua_memory_limit_bytes = 4096;

    expect_lua_diagnostic(
        [&]() { static_cast<void>(execute_lua(runtime, "return string.rep('x', 100000)", session, settings)); },
        "Lua memory limit exceeded");

    settings.lua_memory_limit_bytes = 4 * 1024 * 1024;
    REQUIRE_EQ(execute_lua(runtime, "return 'small'", session, settings).to_string(), std::string("small"));
}

TEST_CASE(LuaRuntime_execute_enforces_render_time_limit) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;
    settings.max_render_time_ms = 0;
    session.start_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);

    expect_lua_diagnostic(
        [&]() { static_cast<void>(execute_lua(runtime, "while true do end return 'x'", session, settings)); },
        "Render time limit exceeded");
}

TEST_CASE(LuaRuntime_sandbox_removes_dangerous_globals) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    REQUIRE(execute_lua(runtime, "return os == nil", session, settings).to_bool());
    REQUIRE(execute_lua(runtime, "return io == nil", session, settings).to_bool());
    REQUIRE(execute_lua(runtime, "return debug == nil", session, settings).to_bool());
    REQUIRE(execute_lua(runtime, "return package == nil", session, settings).to_bool());
    REQUIRE(execute_lua(runtime, "return require == nil", session, settings).to_bool());
    REQUIRE(execute_lua(runtime, "return dofile == nil", session, settings).to_bool());
    REQUIRE(execute_lua(runtime, "return loadfile == nil", session, settings).to_bool());

    expect_lua_diagnostic([&]() { static_cast<void>(execute_lua(runtime, "return os.execute('id')", session, settings)); },
                          "attempt to index a nil value");
}

TEST_CASE(LuaRuntime_sandbox_blocks_loadfile_and_require) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    expect_lua_diagnostic(
        [&]() { static_cast<void>(execute_lua(runtime, "return loadfile('runtime.txt')", session, settings)); },
        "attempt to call a nil value");

    expect_lua_diagnostic([&]() { static_cast<void>(execute_lua(runtime, "return require('os')", session, settings)); },
                          "attempt to call a nil value");
}

TEST_CASE(LuaRuntime_sandbox_still_allows_lua_load_for_safe_chunks) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    REQUIRE_EQ(execute_lua(runtime, "return load('return 41')()", session, settings).to_string(), std::string("41"));
}

TEST_CASE(LuaRuntime_execute_allows_safe_standard_library_features) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    REQUIRE_EQ(execute_lua(runtime, "return string.upper('ada')", session, settings).to_string(), std::string("ADA"));
    REQUIRE_EQ(execute_lua(runtime, "return table.concat({'a', 'b'}, '-')", session, settings).to_string(),
               std::string("a-b"));
    REQUIRE_EQ(execute_lua(runtime, "return math.max(2, 5)", session, settings).to_string(), std::string("5"));
}

TEST_CASE(LuaRuntime_execute_honors_strict_variables_false) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;
    settings.strict_variables = false;

    REQUIRE(!execute_lua(runtime, "return strict_variables", session, settings).to_bool());
}

TEST_CASE(LuaRuntime_execute_reports_span_in_syntax_errors) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::EffectiveSettings settings;

    try {
        static_cast<void>(execute_lua(runtime,
                                      "return (",
                                      session,
                                      settings,
                                      prebyte::LuaChunkMode::Predicate,
                                      "broken.lua",
                                      make_span("broken.lua", 99)));
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError& error) {
        REQUIRE_EQ(error.diagnostic().code, std::string("LUA001"));
        REQUIRE_EQ(error.diagnostic().span.file_path, std::string("broken.lua"));
        REQUIRE_EQ(error.diagnostic().span.start.line, static_cast<std::size_t>(99));
    }
}
