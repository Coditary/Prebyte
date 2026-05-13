#pragma once

#include <filesystem>

#include "config/ConfigTypes.h"
#include "runtime/RenderSession.h"
#include "runtime/Value.h"

struct lua_State;

namespace prebyte {

class LuaValueBridge {
public:
    void push_context(lua_State* state, const EffectiveSettings& settings, const RenderSession& session,
                      const std::filesystem::path& current_file, std::size_t line) const;
    Value read_value(lua_State* state, int index) const;
};

}
