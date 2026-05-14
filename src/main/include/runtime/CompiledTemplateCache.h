#pragma once

#include <filesystem>
#include <chrono>
#include <map>

#include "config/ConfigTypes.h"
#include "runtime/CompiledTemplateProgram.h"

namespace prebyte {

class CompiledTemplateCache {
public:
    static CompiledTemplateCache& instance();

    const CompiledProgram* find(const std::filesystem::path& compiled_path,
                                const EffectiveSettings& settings) const;
    const CompiledProgram* store_loaded(const std::filesystem::path& compiled_path, const CompiledProgram& program,
                                        const EffectiveSettings& settings, std::int64_t compiled_mtime);
    const CompiledProgram* store_in_memory(const std::filesystem::path& compiled_path, const CompiledProgram& program,
                                           const EffectiveSettings& settings);
    void erase(const std::filesystem::path& compiled_path, const EffectiveSettings& settings);
    bool recently_validated(const std::filesystem::path& compiled_path, const EffectiveSettings& settings) const;
    void mark_validated(const std::filesystem::path& compiled_path, const EffectiveSettings& settings);
    std::int64_t compiled_mtime(const std::filesystem::path& compiled_path, const EffectiveSettings& settings) const;

private:
    struct CacheKey {
        std::filesystem::path compiled_path;
        std::string variable_prefix;
        std::string variable_suffix;
        bool replace_tabs = false;
        std::size_t tab_size = 0;

        auto operator<=>(const CacheKey&) const = default;
    };

    struct CacheEntry {
        CompiledProgram program;
        std::int64_t compiled_mtime = 0;
        bool trust_in_memory = false;
        std::chrono::steady_clock::time_point validated_until = std::chrono::steady_clock::time_point::min();
    };

    CacheKey make_key(const std::filesystem::path& compiled_path, const EffectiveSettings& settings) const;

    std::map<CacheKey, CacheEntry> cache_;
};

}
