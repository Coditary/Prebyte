#pragma once

#include <map>
#include <optional>
#include <string>

namespace prebyte {

enum class LuaChunkMode {
    InlineValue,
    Predicate,
    BlockValue,
};

struct LuaChunkKey {
    std::string source;
    LuaChunkMode mode = LuaChunkMode::InlineValue;

    auto operator<=>(const LuaChunkKey&) const = default;
};

class LuaChunkCache {
public:
    std::optional<int> find(const LuaChunkKey& key) const;
    void store(const LuaChunkKey& key, int registry_reference);

private:
    std::map<LuaChunkKey, int> cache_;
};

}
