#include "runtime/LuaSandbox.h"

#include "runtime/LuaHeaders.h"

#include <utility>

namespace prebyte {

LuaSandbox::LuaSandbox(LuaHelperRegistry helper_registry)
    : helper_registry_(std::move(helper_registry)) {}

void LuaSandbox::install(lua_State* state) const {
    luaL_openlibs(state);

    lua_pushnil(state);
    lua_setglobal(state, "os");
    lua_pushnil(state);
    lua_setglobal(state, "io");
    lua_pushnil(state);
    lua_setglobal(state, "debug");
    lua_pushnil(state);
    lua_setglobal(state, "package");
    lua_pushnil(state);
    lua_setglobal(state, "require");
    lua_pushnil(state);
    lua_setglobal(state, "dofile");
    lua_pushnil(state);
    lua_setglobal(state, "loadfile");

    for (const LuaHelperDefinition& helper : helper_registry_.definitions()) {
        lua_pushcfunction(state, helper.function);
        lua_setglobal(state, helper.name.c_str());
    }
}

}
