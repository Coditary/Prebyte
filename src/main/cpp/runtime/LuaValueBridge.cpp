#include "runtime/LuaValueBridge.h"

#include "runtime/LuaHeaders.h"

#include "runtime/BuiltinRegistry.h"

namespace prebyte {

namespace {

void push_args_table(lua_State* state, const std::vector<std::string>& args) {
    lua_newtable(state);
    for (std::size_t index = 0; index < args.size(); ++index) {
        const std::string& value = args[index];
        lua_pushlstring(state, value.data(), value.size());
        lua_rawseti(state, -2, static_cast<lua_Integer>(index));
    }
}

}

void LuaValueBridge::push_context(lua_State* state, const EffectiveSettings& settings, const RenderSession& session,
                                  const std::filesystem::path& current_file, std::size_t line) const {
    lua_newtable(state);

    for (const auto& [name, value] : session.variables.values()) {
        lua_pushlstring(state, value.data(), value.size());
        lua_setfield(state, -2, name.c_str());
    }

    lua_newtable(state);
    for (const auto& [name, value] : session.variables.values()) {
        lua_pushlstring(state, value.data(), value.size());
        lua_setfield(state, -2, name.c_str());
    }
    lua_setfield(state, -2, "vars");

    push_args_table(state, session.args);
    lua_setfield(state, -2, "ARGS");

    BuiltinRegistry builtins;
    SourceSpan span;
    span.file_path = current_file.string();
    span.start.line = line;

    if (const auto time_value = builtins.resolve("__TIME__", span, current_file)) {
        lua_pushlstring(state, time_value->data(), time_value->size());
        lua_setfield(state, -2, "__TIME__");
    }
    lua_pushinteger(state, static_cast<lua_Integer>(line));
    lua_setfield(state, -2, "__LINE__");

    const std::string file_name = current_file.string();
    lua_pushlstring(state, file_name.data(), file_name.size());
    lua_setfield(state, -2, "__FILE__");

    lua_pushboolean(state, settings.strict_variables ? 1 : 0);
    lua_setfield(state, -2, "strict_variables");

    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "_G");
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "_ENV");

    lua_newtable(state);
    lua_pushglobaltable(state);
    lua_setfield(state, -2, "__index");
    lua_pushboolean(state, 0);
    lua_setfield(state, -2, "__metatable");
    lua_setmetatable(state, -2);
}

Value LuaValueBridge::read_value(lua_State* state, int index) const {
    switch (lua_type(state, index)) {
    case LUA_TBOOLEAN:
        return Value(lua_toboolean(state, index) != 0);
    case LUA_TNUMBER:
        return Value(static_cast<double>(lua_tonumber(state, index)));
    case LUA_TSTRING:
        return Value(std::string(lua_tostring(state, index)));
    case LUA_TNIL:
        return Value();
    default:
        return Value(std::string());
    }
}

}
