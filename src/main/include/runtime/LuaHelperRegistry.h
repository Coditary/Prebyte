#pragma once

#include <string>
#include <vector>

struct lua_State;

namespace prebyte {

struct LuaHelperDefinition {
    using Function = int(*)(lua_State* state);

    std::string name;
    std::string signature;
    Function function = nullptr;
};

class LuaHelperRegistry {
public:
    const std::vector<LuaHelperDefinition>& definitions() const;
};

}
