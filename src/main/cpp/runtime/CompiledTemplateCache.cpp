#include "runtime/CompiledTemplateCache.h"

#include "runtime/FileMetadataCache.h"

namespace prebyte {

namespace {

const std::filesystem::path& current_working_directory() {
    static const std::filesystem::path cwd = []() {
        std::error_code error;
        const std::filesystem::path path = std::filesystem::current_path(error);
        return error ? std::filesystem::path{} : path;
    }();
    return cwd;
}

std::filesystem::path normalized_path(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }
    if (path.is_absolute()) {
        return path.lexically_normal();
    }
    if (current_working_directory().empty()) {
        return path.lexically_normal();
    }
    return (current_working_directory() / path).lexically_normal();
}

}

CompiledTemplateCache& CompiledTemplateCache::instance() {
    static CompiledTemplateCache cache;
    return cache;
}

const CompiledProgram* CompiledTemplateCache::find(const std::filesystem::path& compiled_path,
                                                   const EffectiveSettings& settings) const {
    const CacheKey key = make_key(compiled_path, settings);
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return nullptr;
    }
    return &it->second.program;
}

const CompiledProgram* CompiledTemplateCache::store_loaded(const std::filesystem::path& compiled_path,
                                                           const CompiledProgram& program,
                                                           const EffectiveSettings& settings,
                                                           std::int64_t compiled_mtime) {
    const CacheKey key = make_key(compiled_path, settings);
    const auto [it, inserted] = cache_.insert_or_assign(key, CacheEntry{
        .program = program,
        .compiled_mtime = compiled_mtime,
        .trust_in_memory = false,
        .validated_until = std::chrono::steady_clock::now() + FileMetadataCache::ttl(),
    });
    static_cast<void>(inserted);
    return &it->second.program;
}

const CompiledProgram* CompiledTemplateCache::store_in_memory(const std::filesystem::path& compiled_path,
                                                              const CompiledProgram& program,
                                                              const EffectiveSettings& settings) {
    const CacheKey key = make_key(compiled_path, settings);
    const auto [it, inserted] = cache_.insert_or_assign(key, CacheEntry{
        .program = program,
        .compiled_mtime = 0,
        .trust_in_memory = true,
        .validated_until = std::chrono::steady_clock::time_point::max(),
    });
    static_cast<void>(inserted);
    return &it->second.program;
}

void CompiledTemplateCache::erase(const std::filesystem::path& compiled_path, const EffectiveSettings& settings) {
    cache_.erase(make_key(compiled_path, settings));
}

bool CompiledTemplateCache::recently_validated(const std::filesystem::path& compiled_path,
                                               const EffectiveSettings& settings) const {
    const CacheKey key = make_key(compiled_path, settings);
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return false;
    }
    return it->second.trust_in_memory || std::chrono::steady_clock::now() < it->second.validated_until;
}

void CompiledTemplateCache::mark_validated(const std::filesystem::path& compiled_path,
                                           const EffectiveSettings& settings) {
    const CacheKey key = make_key(compiled_path, settings);
    auto it = cache_.find(key);
    if (it == cache_.end() || it->second.trust_in_memory) {
        return;
    }
    it->second.validated_until = std::chrono::steady_clock::now() + FileMetadataCache::ttl();
}

std::int64_t CompiledTemplateCache::compiled_mtime(const std::filesystem::path& compiled_path,
                                                   const EffectiveSettings& settings) const {
    const CacheKey key = make_key(compiled_path, settings);
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return 0;
    }
    return it->second.compiled_mtime;
}

CompiledTemplateCache::CacheKey CompiledTemplateCache::make_key(const std::filesystem::path& compiled_path,
                                                                const EffectiveSettings& settings) const {
    CacheKey key;
    key.compiled_path = normalized_path(compiled_path);
    key.variable_prefix = settings.variable_prefix;
    key.variable_suffix = settings.variable_suffix;
    key.replace_tabs = settings.replace_tabs;
    key.tab_size = settings.tab_size;
    return key;
}

}
