#include "runtime/LuaRuntime.h"

#include "runtime/LuaHeaders.h"

#include <cstdlib>

#include "support/Diagnostic.h"

namespace prebyte {

namespace {

Diagnostic make_lua_error(const std::string& message, const SourceSpan& span) {
    Diagnostic diagnostic;
    diagnostic.code = "LUA001";
    diagnostic.message = message;
    diagnostic.span = span;
    return diagnostic;
}

void instruction_guard(lua_State* state, lua_Debug*) {
    luaL_error(state, "Lua instruction limit exceeded");
}

}

LuaRuntime::LuaRuntime() {
    sandbox_ = LuaSandbox(helper_registry_);
    state_ = lua_newstate(&LuaRuntime::lua_allocator, this);
    if (state_ == nullptr) {
        throw std::runtime_error("Failed to initialize Lua state");
    }
    sandbox_.install(state_);
}

LuaRuntime::~LuaRuntime() {
    if (state_ != nullptr) {
        lua_close(state_);
    }
}

Value LuaRuntime::execute(const std::string& source, LuaChunkMode mode, const EffectiveSettings& settings,
                          const RenderSession& session, const std::filesystem::path& current_file,
                          const SourceSpan& span) const {
    instruction_limit_ = settings.lua_instruction_limit;
    memory_limit_bytes_ = settings.lua_memory_limit_bytes;
    memory_limit_exceeded_ = false;
    RenderSession& mutable_session = const_cast<RenderSession&>(session);
    const int reference = load_chunk(source, mode, span, mutable_session);

    lua_rawgeti(state_, LUA_REGISTRYINDEX, reference);
    value_bridge_.push_context(state_, settings, session, current_file, span.start.line);
    if (lua_setupvalue(state_, -2, 1) == nullptr) {
        lua_pop(state_, 1);
        throw DiagnosticError(make_lua_error("Failed to attach Lua execution environment", span));
    }
    lua_sethook(state_, instruction_guard, LUA_MASKCOUNT, static_cast<int>(instruction_limit_));

    if (lua_pcall(state_, 0, 1, 0) != LUA_OK) {
        const std::string error = take_error_message(state_);
        lua_pop(state_, 1);
        lua_sethook(state_, nullptr, 0, 0);
        lua_gc(state_, LUA_GCCOLLECT, 0);
        throw DiagnosticError(make_lua_error(error, span));
    }

    lua_sethook(state_, nullptr, 0, 0);
    Value value = value_bridge_.read_value(state_, -1);
    lua_pop(state_, 1);
    return value;
}

int LuaRuntime::load_chunk(const std::string& source, LuaChunkMode mode, const SourceSpan& span,
                           RenderSession& session) const {
    const LuaChunkKey key{source, mode};
    if (const auto cached = chunk_cache_.find(key)) {
        ++session.lua_cache_hits;
        return *cached;
    }

    ++session.lua_cache_misses;
    const std::string wrapped = wrap_source(source, mode);
    if (luaL_loadbuffer(state_, wrapped.data(), wrapped.size(), span.file_path.c_str()) != LUA_OK) {
        const std::string error = take_error_message(state_);
        lua_pop(state_, 1);
        lua_gc(state_, LUA_GCCOLLECT, 0);
        throw DiagnosticError(make_lua_error(error, span));
    }

    const int reference = luaL_ref(state_, LUA_REGISTRYINDEX);
    chunk_cache_.store(key, reference);
    return reference;
}

std::string LuaRuntime::wrap_source(const std::string& source, LuaChunkMode mode) const {
    switch (mode) {
    case LuaChunkMode::InlineValue:
    case LuaChunkMode::Predicate:
        return source;
    case LuaChunkMode::BlockValue:
        return source;
    }
    return source;
}

void* LuaRuntime::lua_allocator(void* user_data, void* pointer, std::size_t old_size, std::size_t new_size) {
    auto* runtime = static_cast<LuaRuntime*>(user_data);
    const std::size_t limit = runtime->memory_limit_bytes_;

    if (new_size == 0) {
        if (runtime->memory_bytes_in_use_ >= old_size) {
            runtime->memory_bytes_in_use_ -= old_size;
        } else {
            runtime->memory_bytes_in_use_ = 0;
        }
        std::free(pointer);
        return nullptr;
    }

    const std::size_t growth = pointer == nullptr ? new_size : (new_size > old_size ? new_size - old_size : 0);
    if (growth > 0) {
        if (growth > limit || runtime->memory_bytes_in_use_ > limit - growth) {
            runtime->memory_limit_exceeded_ = true;
            return nullptr;
        }
    }

    void* updated_pointer = pointer == nullptr ? std::malloc(new_size) : std::realloc(pointer, new_size);
    if (updated_pointer == nullptr) {
        return nullptr;
    }

    if (pointer == nullptr) {
        runtime->memory_bytes_in_use_ += new_size;
    } else if (new_size > old_size) {
        runtime->memory_bytes_in_use_ += new_size - old_size;
    } else if (runtime->memory_bytes_in_use_ >= old_size - new_size) {
        runtime->memory_bytes_in_use_ -= old_size - new_size;
    } else {
        runtime->memory_bytes_in_use_ = 0;
    }

    return updated_pointer;
}

std::string LuaRuntime::take_error_message(lua_State* state) const {
    std::string message;
    if (memory_limit_exceeded_) {
        message = "Lua memory limit exceeded";
    } else if (const char* raw_error = lua_tostring(state, -1)) {
        message = raw_error;
    } else {
        message = "Lua execution failed";
    }

    memory_limit_exceeded_ = false;
    return message;
}

}
