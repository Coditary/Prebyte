#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

#include "config/ConfigTypes.h"
#include "runtime/lua/LuaHeaders.h"
#include "runtime/lua/LuaChunkCache.h"
#include "runtime/lua/LuaHelperRegistry.h"
#include "runtime/lua/LuaSandbox.h"
#include "runtime/lua/LuaValueBridge.h"
#include "runtime/core/RenderSession.h"
#include "runtime/core/Value.h"
#include "support/SourceSpan.h"

namespace prebyte {

class LuaRuntime {
public:
    LuaRuntime();
    ~LuaRuntime();

    Value execute(const std::string& source, LuaChunkMode mode, const EffectiveSettings& settings,
                  const RenderSession& session, const std::filesystem::path& current_file,
                  const SourceSpan& span) const;

private:
    static constexpr std::size_t kDefaultMemoryLimitBytes = 4ULL * 1024ULL * 1024ULL;
    static constexpr std::size_t kDefaultInstructionLimit = 100000;
    static constexpr std::size_t kTimeCheckInstructionStep = 1000;

    static void* lua_allocator(void* user_data, void* pointer, std::size_t old_size, std::size_t new_size);
    static void instruction_guard(lua_State* state, lua_Debug* debug);
    void handle_instruction_guard(lua_State* state) const;
    std::string take_error_message(lua_State* state) const;
    int load_chunk(const std::string& source, LuaChunkMode mode, const SourceSpan& span,
                   RenderSession& session) const;
    std::string wrap_source(const std::string& source, LuaChunkMode mode) const;

    lua_State* state_ = nullptr;
    mutable std::size_t instruction_limit_ = kDefaultInstructionLimit;
    mutable std::size_t hook_step_ = kDefaultInstructionLimit;
    mutable std::size_t instructions_executed_ = 0;
    mutable std::size_t memory_limit_bytes_ = kDefaultMemoryLimitBytes;
    mutable std::size_t memory_bytes_in_use_ = 0;
    mutable bool memory_limit_exceeded_ = false;
    mutable const RenderSession* active_session_ = nullptr;
    mutable const EffectiveSettings* active_settings_ = nullptr;
    LuaHelperRegistry helper_registry_;
    LuaSandbox sandbox_;
    LuaValueBridge value_bridge_;
    mutable LuaChunkCache chunk_cache_;
};

}
