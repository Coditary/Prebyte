#pragma once

#include "runtime/lua/LuaHelperRegistry.h"

struct lua_State;

namespace prebyte {

class LuaSandbox {
public:
    explicit LuaSandbox(LuaHelperRegistry helper_registry = {});

    void install(lua_State* state) const;

private:
    LuaHelperRegistry helper_registry_;
};

}
