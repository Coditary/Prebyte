#include "runtime/LuaHelperRegistry.h"

#include "runtime/LuaHeaders.h"

#include "support/TextUtil.h"

#include <cctype>
#include <string>

namespace prebyte {

namespace {

int upper_helper(lua_State* state) {
    std::string value = luaL_checkstring(state, 1);
    for (char& ch : value) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    lua_pushlstring(state, value.data(), value.size());
    return 1;
}

int lower_helper(lua_State* state) {
    std::string value = luaL_checkstring(state, 1);
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    lua_pushlstring(state, value.data(), value.size());
    return 1;
}

int trim_helper(lua_State* state) {
    std::string value = luaL_checkstring(state, 1);
    value = text::trim(std::move(value));
    lua_pushlstring(state, value.data(), value.size());
    return 1;
}

int starts_with_helper(lua_State* state) {
    const std::string value = luaL_checkstring(state, 1);
    const std::string prefix = luaL_checkstring(state, 2);
    lua_pushboolean(state, text::starts_with(value, prefix) ? 1 : 0);
    return 1;
}

int ends_with_helper(lua_State* state) {
    const std::string value = luaL_checkstring(state, 1);
    const std::string suffix = luaL_checkstring(state, 2);
    lua_pushboolean(state, text::ends_with(value, suffix) ? 1 : 0);
    return 1;
}

}

const std::vector<LuaHelperDefinition>& LuaHelperRegistry::definitions() const {
    static const std::vector<LuaHelperDefinition> definitions = {
        {.name = "upper", .signature = "upper(value)", .function = upper_helper},
        {.name = "lower", .signature = "lower(value)", .function = lower_helper},
        {.name = "trim", .signature = "trim(value)", .function = trim_helper},
        {.name = "starts_with", .signature = "starts_with(value, prefix)", .function = starts_with_helper},
        {.name = "ends_with", .signature = "ends_with(value, suffix)", .function = ends_with_helper},
    };
    return definitions;
}

}
