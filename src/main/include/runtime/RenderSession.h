#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <set>
#include <vector>

#include "config/ConfigTypes.h"
#include "runtime/VariableStore.h"

namespace prebyte {

struct RenderSession {
    ResolvedConfiguration configuration;
    VariableStore variables;
    std::vector<std::string> args;
    std::set<std::string> ignore_names;
    std::vector<std::filesystem::path> include_stack;
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    mutable std::shared_ptr<class LuaRuntime> lua_runtime;
    mutable std::size_t lua_cache_hits = 0;
    mutable std::size_t lua_cache_misses = 0;
};

}
