#include "TestHarness.h"

#include "datatypes/Data.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/ValueResolver.h"
#include "support/Diagnostic.h"
#include "support/SourceSpan.h"

#include <filesystem>

namespace {

prebyte::SourceSpan make_span(const char* file_path = "resolver.txt", std::size_t line = 3) {
    prebyte::SourceSpan span;
    span.file_path = file_path;
    span.start.line = line;
    return span;
}

prebyte::Value resolve_name(prebyte::ValueResolver& resolver,
                            const std::string& name,
                            prebyte::RenderSession& session,
                            const prebyte::EffectiveSettings& settings = {},
                            const std::filesystem::path& current_file = "resolver.txt") {
    return resolver.resolve_identifier(name, make_span(), settings, session, current_file);
}

prebyte::Value resolve_member(prebyte::ValueResolver& resolver,
                            const prebyte::Value& base,
                            std::string_view member,
                            const prebyte::EffectiveSettings& settings = {}) {
    return resolver.resolve_member(base, member, make_span(), settings);
}

prebyte::Value resolve_index(prebyte::ValueResolver& resolver,
                             const prebyte::Value& base,
                             const prebyte::Value& index,
                             const prebyte::EffectiveSettings& settings = {}) {
    return resolver.resolve_index(base, index, make_span(), settings);
}

void expect_runtime_error(const auto& callable,
                          const std::string& expected_message_substring,
                          const std::string& expected_code = "RUNTIME001") {
    try {
        callable();
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError& error) {
        REQUIRE_EQ(error.diagnostic().code, expected_code);
        REQUIRE(error.diagnostic().message.find(expected_message_substring) != std::string::npos);
    }
}

prebyte::Value make_user_object() {
    prebyte::Data::Map address;
    address["city"] = prebyte::Data("London");
    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");
    user["address"] = prebyte::Data(std::move(address));
    return prebyte::Value::object(user);
}

}

TEST_CASE(ValueResolver_resolve_identifier_reads_variables_and_builtins) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");

    REQUIRE_EQ(resolve_name(resolver, "name", session).to_string(), std::string("Ada"));
    REQUIRE_EQ(resolve_name(resolver, "__LINE__", session, {}, "resolver.txt").to_string(), std::string("3"));
}

TEST_CASE(ValueResolver_resolve_identifier_reads_render_args) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.args = {"alpha", "beta"};

    REQUIRE_EQ(resolve_name(resolver, "ARGS[0]", session).to_string(), std::string("alpha"));
    REQUIRE_EQ(resolve_name(resolver, "ARGS[1]", session).to_string(), std::string("beta"));
}

TEST_CASE(ValueResolver_resolve_identifier_resolves_member_paths) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.variables.set_value("user", make_user_object());

    REQUIRE_EQ(resolve_name(resolver, "user.name", session).to_string(), std::string("Ada"));
    REQUIRE_EQ(resolve_name(resolver, "user.address.city", session).to_string(), std::string("London"));
}

TEST_CASE(ValueResolver_resolve_identifier_prefers_scoped_values_over_variables) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.variables.set("name", "Global");

    session.set_local_value("name", prebyte::Value(std::string("Local")));
    REQUIRE_EQ(resolve_name(resolver, "name", session).to_string(), std::string("Local"));

    prebyte::RenderSession::LoopFrame frame;
    frame.binding_name_0 = "name";
    frame.binding_value_0 = prebyte::Value(std::string("Loop"));
    frame.loop_index0 = 0;
    frame.loop_size = 2;
    session.push_loop_frame(std::move(frame));
    REQUIRE_EQ(resolve_name(resolver, "name", session).to_string(), std::string("Loop"));
}

TEST_CASE(ValueResolver_resolve_identifier_reads_loop_metadata) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;

    prebyte::RenderSession::LoopFrame frame;
    frame.binding_name_0 = "item";
    frame.binding_value_0 = prebyte::Value("Ada");
    frame.loop_index0 = 1;
    frame.loop_size = 3;
    session.push_loop_frame(std::move(frame));

    REQUIRE_EQ(resolve_name(resolver, "loop.index", session).to_string(), std::string("2"));
    REQUIRE_EQ(resolve_name(resolver, "loop.index0", session).to_string(), std::string("1"));
    REQUIRE(!resolve_name(resolver, "loop.first", session).to_bool());
    REQUIRE(!resolve_name(resolver, "loop.last", session).to_bool());
}

TEST_CASE(ValueResolver_resolve_identifier_honors_case_sensitive_variables) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.variables.set("Name", "Ada");

    prebyte::EffectiveSettings sensitive;
    sensitive.case_sensitive_variables = true;
    REQUIRE_EQ(resolve_name(resolver, "Name", session, sensitive).to_string(), std::string("Ada"));
    REQUIRE(resolve_name(resolver, "name", session, sensitive).is_null());

    prebyte::EffectiveSettings insensitive;
    insensitive.case_sensitive_variables = false;
    REQUIRE_EQ(resolve_name(resolver, "name", session, insensitive).to_string(), std::string("Ada"));
}

TEST_CASE(ValueResolver_resolve_identifier_returns_empty_string_for_ignored_names) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.variables.set("secret", "Hidden");
    session.ignore_names.insert("secret");

    const prebyte::Value value = resolve_name(resolver, "secret", session);
    REQUIRE(!value.is_null());
    REQUIRE_EQ(value.to_string(), std::string());
}

TEST_CASE(ValueResolver_resolve_identifier_applies_trim_and_max_length) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.variables.set("label", "  Ada  ");

    prebyte::EffectiveSettings settings;
    settings.trim = true;
    settings.has_max_variable_length = true;
    settings.max_variable_length = 3;

    REQUIRE_EQ(resolve_name(resolver, "label", session, settings).to_string(), std::string("Ada"));
    REQUIRE_EQ(resolver.normalize_string("  Hello World  ", settings), std::string("Hel"));
}

TEST_CASE(ValueResolver_resolve_identifier_reads_allowed_environment_variables) {
    prebyte::test::ScopedEnvironmentVariable allowed_env("PREBYTE_VALUE_RESOLVER_ENV", "Grace");

    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;

    prebyte::EffectiveSettings settings;
    settings.allow_env = true;

    REQUIRE_EQ(resolve_name(resolver, "PREBYTE_VALUE_RESOLVER_ENV", session, settings).to_string(),
               std::string("Grace"));
}

TEST_CASE(ValueResolver_resolve_identifier_uses_default_value_when_not_strict) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;

    prebyte::EffectiveSettings settings;
    settings.strict_variables = false;
    settings.default_variable_value = "Fallback";

    REQUIRE_EQ(resolve_name(resolver, "missing", session, settings).to_string(), std::string("Fallback"));

    settings.default_variable_value = "  padded  ";
    settings.trim = true;
    REQUIRE_EQ(resolve_name(resolver, "missing", session, settings).to_string(), std::string("padded"));
}

TEST_CASE(ValueResolver_resolve_identifier_returns_empty_value_without_default) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;

    prebyte::EffectiveSettings settings;
    settings.strict_variables = false;

    REQUIRE(resolve_name(resolver, "missing", session, settings).is_null());
    REQUIRE_EQ(resolve_name(resolver, "missing", session, settings).to_string(), std::string());
}

TEST_CASE(ValueResolver_resolve_identifier_rejects_unknown_variables_when_strict) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;

    prebyte::EffectiveSettings settings;
    settings.strict_variables = true;

    expect_runtime_error([&]() { static_cast<void>(resolve_name(resolver, "missing", session, settings)); },
                         "Unknown variable: missing");
}

TEST_CASE(ValueResolver_resolve_identifier_rejects_invalid_args_references) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.args = {"only"};

    expect_runtime_error([&]() { static_cast<void>(resolve_name(resolver, "ARGS", session)); },
                         "ARGS must be accessed as ARGS[index]");

    expect_runtime_error([&]() { static_cast<void>(resolve_name(resolver, "ARGS[x]", session)); },
                         "Invalid ARGS index");

    expect_runtime_error([&]() { static_cast<void>(resolve_name(resolver, "ARGS[-1]", session)); },
                         "Invalid ARGS index");

    expect_runtime_error([&]() { static_cast<void>(resolve_name(resolver, "ARGS[9]", session)); },
                         "ARGS index out of range");
}

TEST_CASE(ValueResolver_resolve_identifier_rejects_forbidden_environment_variables) {
    prebyte::test::ScopedEnvironmentVariable blocked_env("PREBYTE_VALUE_RESOLVER_BLOCKED", "Secret");

    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;

    prebyte::EffectiveSettings settings;
    settings.allow_env = true;
    settings.forbidden_env_vars.insert("PREBYTE_VALUE_RESOLVER_BLOCKED");

    expect_runtime_error(
        [&]() { static_cast<void>(resolve_name(resolver, "PREBYTE_VALUE_RESOLVER_BLOCKED", session, settings)); },
        "Access to forbidden environment variable");
}

TEST_CASE(ValueResolver_resolve_identifier_rejects_invalid_member_paths) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.variables.set_value("user", make_user_object());

    expect_runtime_error([&]() { static_cast<void>(resolve_name(resolver, ".name", session)); }, "Invalid member access");
    expect_runtime_error([&]() { static_cast<void>(resolve_name(resolver, "user.", session)); }, "Invalid member access");
    expect_runtime_error([&]() { static_cast<void>(resolve_name(resolver, "user..city", session)); },
                         "Invalid member access");
}

TEST_CASE(ValueResolver_resolve_identifier_reports_missing_nested_members_when_strict) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.variables.set_value("user", make_user_object());

    prebyte::EffectiveSettings settings;
    settings.strict_variables = true;

    expect_runtime_error([&]() { static_cast<void>(resolve_name(resolver, "user.missing", session, settings)); },
                         "Unknown variable: user.missing");
    expect_runtime_error(
        [&]() { static_cast<void>(resolve_name(resolver, "user.address.country", session, settings)); },
        "Unknown variable: user.address.country");
}

TEST_CASE(ValueResolver_resolve_identifier_rejects_member_access_on_non_object_paths) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.variables.set_value("items", prebyte::Value::list(prebyte::Data::Array{prebyte::Data("Ada")}));

    expect_runtime_error([&]() { static_cast<void>(resolve_name(resolver, "items.name", session)); },
                         "Cannot access member 'name' on non-object value");
}

TEST_CASE(ValueResolver_resolve_member_reads_object_fields) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    const prebyte::Value user = make_user_object();

    REQUIRE_EQ(resolve_member(resolver, user, "name").to_string(), std::string("Ada"));
}

TEST_CASE(ValueResolver_resolve_member_handles_missing_and_invalid_access) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    const prebyte::Value user = make_user_object();
    const prebyte::Value list = prebyte::Value::list(prebyte::Data::Array{prebyte::Data("Ada")});

    prebyte::EffectiveSettings strict;
    strict.strict_variables = true;
    expect_runtime_error([&]() { static_cast<void>(resolve_member(resolver, user, "missing", strict)); },
                         "Unknown variable: missing");

    prebyte::EffectiveSettings lenient;
    lenient.strict_variables = false;
    lenient.default_variable_value = "Fallback";
    REQUIRE_EQ(resolve_member(resolver, user, "missing", lenient).to_string(), std::string("Fallback"));
    REQUIRE_EQ(resolve_member(resolver, prebyte::Value(), "missing", lenient).to_string(), std::string("Fallback"));

    expect_runtime_error([&]() { static_cast<void>(resolve_member(resolver, list, "name", strict)); },
                         "Cannot access member 'name' on non-object value");
}

TEST_CASE(ValueResolver_resolve_index_reads_list_and_object_entries) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);

    const prebyte::Value list = prebyte::Value::list(prebyte::Data::Array{
        prebyte::Data("Ada"),
        prebyte::Data("Grace"),
    });
    const prebyte::Value object = prebyte::Value::object(prebyte::Data::Map{
        {"name", prebyte::Data("Ada")},
        {"2", prebyte::Data("two")},
    });

    REQUIRE_EQ(resolve_index(resolver, list, prebyte::Value(0.0)).to_string(), std::string("Ada"));
    REQUIRE_EQ(resolve_index(resolver, list, prebyte::Value(1.0)).to_string(), std::string("Grace"));
    REQUIRE_EQ(resolve_index(resolver, object, prebyte::Value(std::string("name"))).to_string(), std::string("Ada"));
    REQUIRE_EQ(resolve_index(resolver, object, prebyte::Value(2.0)).to_string(), std::string("two"));
}

TEST_CASE(ValueResolver_resolve_index_handles_missing_entries) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);

    const prebyte::Value list = prebyte::Value::list(prebyte::Data::Array{prebyte::Data("Ada")});
    const prebyte::Value object = prebyte::Value::object(prebyte::Data::Map{{"name", prebyte::Data("Ada")}});

    prebyte::EffectiveSettings lenient;
    lenient.strict_variables = false;
    lenient.default_variable_value = "Fallback";

    REQUIRE_EQ(resolve_index(resolver, list, prebyte::Value(9.0), lenient).to_string(), std::string("Fallback"));
    REQUIRE_EQ(resolve_index(resolver, object, prebyte::Value(std::string("missing")), lenient).to_string(),
               std::string("Fallback"));
    REQUIRE_EQ(resolve_index(resolver, prebyte::Value(), prebyte::Value(0.0), lenient).to_string(),
               std::string("Fallback"));

    prebyte::EffectiveSettings strict;
    strict.strict_variables = true;
    expect_runtime_error([&]() { static_cast<void>(resolve_index(resolver, list, prebyte::Value(9.0), strict)); },
                         "Unknown variable: [index]");
    expect_runtime_error(
        [&]() { static_cast<void>(resolve_index(resolver, object, prebyte::Value(std::string("missing")), strict)); },
        "Unknown variable: missing");
}

TEST_CASE(ValueResolver_resolve_index_rejects_invalid_indexes_and_targets) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);

    const prebyte::Value list = prebyte::Value::list(prebyte::Data::Array{prebyte::Data("Ada")});
    const prebyte::Value object = prebyte::Value::object(prebyte::Data::Map{{"name", prebyte::Data("Ada")}});
    const prebyte::Value scalar = prebyte::Value(std::string("plain"));

    expect_runtime_error([&]() { static_cast<void>(resolve_index(resolver, list, prebyte::Value(-1.0))); },
                         "List index must be non-negative integer");
    expect_runtime_error([&]() { static_cast<void>(resolve_index(resolver, list, prebyte::Value(std::string("x")))); },
                         "List index must be non-negative integer");
    expect_runtime_error(
        [&]() { static_cast<void>(resolve_index(resolver, object, prebyte::Value::list(prebyte::Data::Array{}))); },
        "Object key must be scalar value");
    expect_runtime_error(
        [&]() { static_cast<void>(resolve_index(resolver, object, prebyte::Value::object(prebyte::Data::Map{}))); },
        "Object key must be scalar value");
    expect_runtime_error([&]() { static_cast<void>(resolve_index(resolver, scalar, prebyte::Value(0.0))); },
                         "Cannot index non-container value");
}

TEST_CASE(ValueResolver_resolve_identifier_prefers_builtins_over_variables) {
    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;
    session.variables.set("__LINE__", "999");

    REQUIRE_EQ(resolve_name(resolver, "__LINE__", session, {}, "resolver.txt").to_string(), std::string("3"));
}

TEST_CASE(ValueResolver_resolve_identifier_skips_env_when_not_allowed) {
    prebyte::test::ScopedEnvironmentVariable allowed_env("PREBYTE_VALUE_RESOLVER_HIDDEN", "Secret");

    prebyte::BuiltinRegistry builtins;
    prebyte::ValueResolver resolver(builtins);
    prebyte::RenderSession session;

    prebyte::EffectiveSettings settings;
    settings.allow_env = false;
    settings.strict_variables = false;
    settings.default_variable_value = "Fallback";

    REQUIRE_EQ(resolve_name(resolver, "PREBYTE_VALUE_RESOLVER_HIDDEN", session, settings).to_string(),
               std::string("Fallback"));
}
