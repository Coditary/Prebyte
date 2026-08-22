#include "TestHarness.h"

#include "datatypes/Data.h"
#include "runtime/lua/LuaHeaders.h"
#include "runtime/lua/LuaRuntime.h"
#include "runtime/lua/LuaValueBridge.h"
#include "support/SourceSpan.h"

namespace {

struct LuaStateGuard {
    lua_State* state = luaL_newstate();

    ~LuaStateGuard() {
        if (state != nullptr) {
            lua_close(state);
        }
    }
};

prebyte::SourceSpan make_span(std::size_t line = 7) {
    prebyte::SourceSpan span;
    span.file_path = "bridge.txt";
    span.start.line = line;
    return span;
}

prebyte::Value read_lua_value(prebyte::LuaValueBridge& bridge, lua_State* state, const auto& push_value) {
    push_value(state);
    const prebyte::Value value = bridge.read_value(state, -1);
    lua_pop(state, 1);
    return value;
}

}

TEST_CASE(LuaValueBridge_read_value_converts_lua_scalars_and_nil) {
    prebyte::LuaValueBridge bridge;
    LuaStateGuard guard;

    REQUIRE(read_lua_value(bridge, guard.state, [](lua_State* state) { lua_pushnil(state); }).is_null());
    REQUIRE(read_lua_value(bridge, guard.state, [](lua_State* state) { lua_pushboolean(state, 1); }).to_bool());
    REQUIRE(!read_lua_value(bridge, guard.state, [](lua_State* state) { lua_pushboolean(state, 0); }).to_bool());
    REQUIRE_EQ(read_lua_value(bridge, guard.state, [](lua_State* state) { lua_pushnumber(state, 3.5); }).to_string(),
               std::string("3.5"));
    REQUIRE_EQ(read_lua_value(bridge, guard.state,
                             [](lua_State* state) { lua_pushlstring(state, "hello", 5); }).to_string(),
               std::string("hello"));
}

TEST_CASE(LuaValueBridge_read_value_converts_array_like_tables) {
    prebyte::LuaValueBridge bridge;
    LuaStateGuard guard;

    const prebyte::Value value = read_lua_value(bridge, guard.state, [](lua_State* state) {
        lua_newtable(state);
        lua_pushlstring(state, "Ada", 3);
        lua_rawseti(state, -2, 1);
        lua_pushlstring(state, "Grace", 5);
        lua_rawseti(state, -2, 2);
    });

    REQUIRE(value.is_list());
    REQUIRE_EQ(value.length(), static_cast<std::size_t>(2));
    REQUIRE_EQ(value.index(0)->to_string(), std::string("Ada"));
    REQUIRE_EQ(value.index(1)->to_string(), std::string("Grace"));
}

TEST_CASE(LuaValueBridge_read_value_converts_object_tables) {
    prebyte::LuaValueBridge bridge;
    LuaStateGuard guard;

    const prebyte::Value value = read_lua_value(bridge, guard.state, [](lua_State* state) {
        lua_newtable(state);
        lua_pushboolean(state, 1);
        lua_setfield(state, -2, "active");
        lua_pushnumber(state, 2);
        lua_setfield(state, -2, "count");
        lua_pushlstring(state, "Ada", 3);
        lua_setfield(state, -2, "name");
    });

    REQUIRE(value.is_object());
    REQUIRE(value.member("active")->to_bool());
    REQUIRE_EQ(value.member("count")->to_string(), std::string("2"));
    REQUIRE_EQ(value.member("name")->to_string(), std::string("Ada"));
}

TEST_CASE(LuaValueBridge_read_value_treats_sparse_tables_as_objects) {
    prebyte::LuaValueBridge bridge;
    LuaStateGuard guard;

    const prebyte::Value sparse = read_lua_value(bridge, guard.state, [](lua_State* state) {
        lua_newtable(state);
        lua_pushlstring(state, "Grace", 5);
        lua_rawseti(state, -2, 2);
    });

    REQUIRE(sparse.is_object());
    REQUIRE(!sparse.is_list());
    REQUIRE(sparse.member("2").has_value());
    REQUIRE_EQ(sparse.member("2")->to_string(), std::string("Grace"));
}

TEST_CASE(LuaValueBridge_read_value_converts_numeric_object_keys_to_strings) {
    prebyte::LuaValueBridge bridge;
    LuaStateGuard guard;

    const prebyte::Value value = read_lua_value(bridge, guard.state, [](lua_State* state) {
        lua_newtable(state);
        lua_pushlstring(state, "kept", 4);
        lua_setfield(state, -2, "kept");
        lua_pushinteger(state, 42);
        lua_pushlstring(state, "hidden", 6);
        lua_rawset(state, -3);
    });

    REQUIRE(value.is_object());
    REQUIRE(value.member("kept").has_value());
    REQUIRE(value.member("42").has_value());
    REQUIRE_EQ(value.member("42")->to_string(), std::string("hidden"));
    REQUIRE(!value.member("hidden").has_value());
}

TEST_CASE(LuaValueBridge_read_value_converts_unsupported_lua_types_to_empty_string) {
    prebyte::LuaValueBridge bridge;
    LuaStateGuard guard;

    const prebyte::Value value = read_lua_value(bridge, guard.state, [](lua_State* state) {
        lua_pushcfunction(state, [](lua_State*) -> int { return 0; });
    });

    REQUIRE(!value.is_null());
    REQUIRE_EQ(value.to_string(), std::string());
}

TEST_CASE(LuaValueBridge_push_context_exposes_variables_args_vars_and_settings) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");
    session.variables.set_value("count", prebyte::Value(2.0));
    session.args = {"first", "second"};

    prebyte::EffectiveSettings settings;
    settings.strict_variables = true;

    const prebyte::Value value = runtime.execute(
        "return name .. '|' .. tostring(count) .. '|' .. vars.name .. '|' .. ARGS[0] .. '|' .. ARGS[1] .. '|' .. "
        "tostring(strict_variables) .. '|' .. __LINE__ .. '|' .. __FILE__",
        prebyte::LuaChunkMode::Predicate,
        settings,
        session,
        "bridge.txt",
        make_span(9));

    REQUIRE_EQ(value.to_string(), std::string("Ada|2.0|Ada|first|second|true|9|bridge.txt"));
}

TEST_CASE(LuaValueBridge_round_trips_nested_structured_values) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;
    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");
    user["active"] = prebyte::Data(true);
    prebyte::Data::Array tags;
    tags.push_back(prebyte::Data("admin"));
    tags.push_back(prebyte::Data("editor"));
    session.variables.set_value("user", prebyte::Value::object(user));
    session.variables.set_value("tags", prebyte::Value::list(tags));

    const prebyte::Value from_lua = runtime.execute(
        R"(return {
            user = { name = user.name, active = user.active },
            tags = { tags[1], tags[2] },
            total = 2
        })",
        prebyte::LuaChunkMode::Predicate,
        prebyte::EffectiveSettings{},
        session,
        "bridge.txt",
        make_span());

    REQUIRE(from_lua.is_object());
    REQUIRE(from_lua.member("total")->to_string() == std::string("2"));
    REQUIRE(from_lua.member("user")->member("name")->to_string() == std::string("Ada"));
    REQUIRE(from_lua.member("user")->member("active")->to_bool());
    REQUIRE(from_lua.member("tags")->is_list());
    REQUIRE_EQ(from_lua.member("tags")->length(), static_cast<std::size_t>(2));
    REQUIRE_EQ(from_lua.member("tags")->index(0)->to_string(), std::string("admin"));
    REQUIRE_EQ(from_lua.member("tags")->index(1)->to_string(), std::string("editor"));
}

TEST_CASE(LuaValueBridge_read_value_rejects_non_integer_array_keys) {
    prebyte::LuaValueBridge bridge;
    LuaStateGuard guard;

    const prebyte::Value mixed = read_lua_value(bridge, guard.state, [](lua_State* state) {
        lua_newtable(state);
        lua_pushlstring(state, "Ada", 3);
        lua_rawseti(state, -2, 1);
        lua_pushlstring(state, "extra", 5);
        lua_setfield(state, -2, "name");
    });

    REQUIRE(mixed.is_object());
    REQUIRE(mixed.member("1").has_value());
    REQUIRE(mixed.member("name").has_value());

    const prebyte::Value zero_indexed = read_lua_value(bridge, guard.state, [](lua_State* state) {
        lua_newtable(state);
        lua_pushlstring(state, "zero", 4);
        lua_rawseti(state, -2, 0);
    });

    REQUIRE(zero_indexed.is_object());
    REQUIRE(zero_indexed.member("0").has_value());
}

TEST_CASE(LuaValueBridge_read_value_converts_float_table_keys) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;

    const prebyte::Value value = runtime.execute(R"(return { [1.5] = "half" })", prebyte::LuaChunkMode::Predicate,
                                                 prebyte::EffectiveSettings{}, session, "bridge.txt", make_span());

    REQUIRE(value.is_object());
    REQUIRE(value.member(std::to_string(1.5)).has_value());
    REQUIRE_EQ(value.member(std::to_string(1.5))->to_string(), std::string("half"));
}

TEST_CASE(LuaValueBridge_push_context_respects_strict_variables_flag) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;

    prebyte::EffectiveSettings strict_off;
    strict_off.strict_variables = false;

    const prebyte::Value value = runtime.execute("return strict_variables", prebyte::LuaChunkMode::Predicate,
                                                 strict_off, session, "bridge.txt", make_span());

    REQUIRE(!value.to_bool());
}

TEST_CASE(LuaValueBridge_execute_returns_lua_nil_as_null_value) {
    prebyte::LuaRuntime runtime;
    prebyte::RenderSession session;

    const prebyte::Value value = runtime.execute("return nil", prebyte::LuaChunkMode::Predicate,
                                                 prebyte::EffectiveSettings{}, session, "bridge.txt", make_span());

    REQUIRE(value.is_null());
}
