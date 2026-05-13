#include "runtime/LuaChunkCache.h"

namespace prebyte {

std::optional<int> LuaChunkCache::find(const LuaChunkKey& key) const {
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void LuaChunkCache::store(const LuaChunkKey& key, int registry_reference) {
    cache_[key] = registry_reference;
}

}
