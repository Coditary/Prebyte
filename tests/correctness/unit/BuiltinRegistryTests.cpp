#include "TestHarness.h"

#include "runtime/BuiltinRegistry.h"

#include <filesystem>

TEST_CASE(BuiltinRegistry_resolve_file_path_builtins) {
    prebyte::BuiltinRegistry builtins;
    prebyte::RenderSession session;
    prebyte::SourceSpan span;
    span.start.line = 7;

    const std::filesystem::path path = std::filesystem::path("tests/fixtures/example/sample.pbt");
    const std::filesystem::path absolute = std::filesystem::absolute(path).lexically_normal();

    REQUIRE_EQ(*builtins.resolve("__FILE__", span, absolute, session), absolute.string());
    REQUIRE_EQ(*builtins.resolve("__FILENAME__", span, absolute, session), std::string("sample"));
    REQUIRE_EQ(*builtins.resolve("__EXTENSION__", span, absolute, session), std::string("pbt"));
    REQUIRE_EQ(*builtins.resolve("__DIR__", span, absolute, session), absolute.parent_path().string());
    REQUIRE_EQ(*builtins.resolve("__LINE__", span, absolute, session), std::string("7"));
}

TEST_CASE(BuiltinRegistry_resolve_inline_path_builtins_to_empty_strings) {
    prebyte::BuiltinRegistry builtins;
    prebyte::RenderSession session;
    prebyte::SourceSpan span;

    REQUIRE_EQ(*builtins.resolve("__FILENAME__", span, {}, session), std::string());
    REQUIRE_EQ(*builtins.resolve("__DIR__", span, {}, session), std::string());
    REQUIRE_EQ(*builtins.resolve("__EXTENSION__", span, {}, session), std::string());
}

TEST_CASE(BuiltinRegistry_resolve_time_builtins_with_expected_shapes) {
    prebyte::BuiltinRegistry builtins;
    prebyte::RenderSession session;
    prebyte::SourceSpan span;

    const std::string date = *builtins.resolve("__DATE__", span, {}, session);
    const std::string timestamp = *builtins.resolve("__TIMESTAMP__", span, {}, session);
    const std::string year = *builtins.resolve("__YEAR__", span, {}, session);
    const std::string month = *builtins.resolve("__MONTH__", span, {}, session);
    const std::string day = *builtins.resolve("__DAY__", span, {}, session);
    const std::string epoch = *builtins.resolve("__UNIX_EPOCH__", span, {}, session);

    REQUIRE_EQ(date.size(), static_cast<std::size_t>(10));
    REQUIRE_EQ(date[4], '-');
    REQUIRE_EQ(date[7], '-');
    REQUIRE_EQ(timestamp.size(), static_cast<std::size_t>(19));
    REQUIRE_EQ(timestamp[4], '-');
    REQUIRE_EQ(timestamp[7], '-');
    REQUIRE_EQ(timestamp[10], 'T');
    REQUIRE_EQ(year.size(), static_cast<std::size_t>(4));
    REQUIRE_EQ(month.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(day.size(), static_cast<std::size_t>(2));
    REQUIRE(!epoch.empty());
}

TEST_CASE(BuiltinRegistry_dynamic_builtins_are_cached_per_session) {
    prebyte::BuiltinRegistry builtins;
    prebyte::SourceSpan span;
    prebyte::RenderSession first;
    prebyte::RenderSession second;

    const std::string first_uuid = *builtins.resolve("__UUID__", span, {}, first);
    const std::string first_uuid_again = *builtins.resolve("__UUID__", span, {}, first);
    const std::string first_random = *builtins.resolve("__RANDOM__", span, {}, first);
    const std::string first_random_again = *builtins.resolve("__RANDOM__", span, {}, first);
    const std::string second_uuid = *builtins.resolve("__UUID__", span, {}, second);

    REQUIRE_EQ(first_uuid, first_uuid_again);
    REQUIRE_EQ(first_random, first_random_again);
    REQUIRE(!first_uuid.empty());
    REQUIRE(!first_random.empty());
    REQUIRE(!second_uuid.empty());
}

TEST_CASE(BuiltinRegistry_resolve_system_builtins) {
    prebyte::BuiltinRegistry builtins;
    prebyte::RenderSession session;
    prebyte::SourceSpan span;
    const std::string working_dir = *builtins.resolve("__WORKING_DIR__", span, {}, session);
    const std::string os = *builtins.resolve("__OS__", span, {}, session);

    REQUIRE_EQ(working_dir, std::filesystem::current_path().lexically_normal().string());
    REQUIRE(os == "linux" || os == "macos" || os == "windows" || os == "unknown");
    REQUIRE(builtins.resolve("__USER__", span, {}, session).has_value());
    REQUIRE(builtins.resolve("__HOST__", span, {}, session).has_value());
}
