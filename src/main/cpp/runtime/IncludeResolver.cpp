#include "runtime/IncludeResolver.h"

#include "runtime/FileMetadataCache.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "support/Diagnostic.h"

#include <algorithm>

#include <cstdlib>

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

std::filesystem::path shared_include_root() {
#if defined(_WIN32)
    if (const char* local_app_data = std::getenv("LOCALAPPDATA"); local_app_data != nullptr && *local_app_data != '\0') {
        return std::filesystem::path(local_app_data) / "Prebyte" / "share";
    }
    if (const char* app_data = std::getenv("APPDATA"); app_data != nullptr && *app_data != '\0') {
        return std::filesystem::path(app_data) / "Prebyte" / "share";
    }
    if (const char* profile = std::getenv("USERPROFILE"); profile != nullptr && *profile != '\0') {
        return std::filesystem::path(profile) / "AppData" / "Local" / "Prebyte" / "share";
    }
    return std::filesystem::path("Prebyte") / "share";
#else
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".local/share/prebyte";
    }
    return ".local/share/prebyte";
#endif
}

std::filesystem::path canonical_path(const std::filesystem::path& path) {
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

Diagnostic make_include_error(const std::string& message, const std::filesystem::path& path,
                              const RenderSession& session) {
    Diagnostic diagnostic;
    diagnostic.code = "RUNTIME002";
    diagnostic.message = message;
    diagnostic.span.file_path = path.string();
    for (const auto& include : session.include_stack) {
        diagnostic.include_chain.push_back(include.string());
    }
    return diagnostic;
}

bool is_explicit_relative(const std::string& include_path) {
    return include_path.starts_with("./") || include_path.starts_with("../")
        || include_path.starts_with(".\\") || include_path.starts_with("..\\");
}

bool path_exists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

bool path_is_directory(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_directory(path, error) && !error;
}

bool try_accept_include(const std::filesystem::path& physical_path, const std::filesystem::path& logical_path,
                        ResolvedIncludeKind kind, RenderSession& session, ResolvedInclude& resolved) {
    const std::filesystem::path absolute = canonical_path(physical_path);
    const std::filesystem::path cycle_key = canonical_path(logical_path.empty() ? physical_path : logical_path);
    if (session.contains_include(cycle_key)) {
        throw DiagnosticError(make_include_error("Include cycle detected", cycle_key, session));
    }

    resolved.path = absolute;
    resolved.logical_path = cycle_key;
    resolved.kind = kind;
    session.push_include(resolved.logical_path);
    return true;
}

bool try_file_variant(const std::filesystem::path& physical_path, const std::filesystem::path& logical_path,
                      ResolvedIncludeKind kind, const EffectiveSettings& settings,
                      RenderSession& session, ResolvedInclude& resolved) {
    if (kind == ResolvedIncludeKind::Compiled) {
        CompiledTemplateSerializer serializer;
        if (const CompiledProgram* compiled = serializer.try_load_valid(physical_path, settings)) {
            if (!try_accept_include(physical_path, logical_path, kind, session, resolved)) {
                return false;
            }
            resolved.compiled_program = compiled;
            return true;
        }
        return false;
    }

    InputBuffer source;
    try {
        source = InputBuffer::from_file(physical_path);
    } catch (const std::exception&) {
        return false;
    }
    if (!try_accept_include(physical_path, logical_path, kind, session, resolved)) {
        return false;
    }
    resolved.source = std::move(source);
    return true;
}

bool try_target_path(const std::filesystem::path& logical, const EffectiveSettings& settings,
                     RenderSession& session, ResolvedInclude& resolved) {
    const std::filesystem::path pbc = logical.string() + ".pbc";
    if (try_file_variant(pbc, logical, ResolvedIncludeKind::Compiled, settings, session, resolved)) {
        return true;
    }

    const std::filesystem::path pbt = logical.string() + ".pbt";
    if (try_file_variant(pbt, logical, ResolvedIncludeKind::Source, settings, session, resolved)) {
        return true;
    }

    if (try_file_variant(logical, logical, ResolvedIncludeKind::Source, settings, session, resolved)) {
        return true;
    }

    if (path_is_directory(logical)) {
        const std::filesystem::path index_logical = logical / "index";
        if (try_file_variant(index_logical.string() + ".pbc", index_logical, ResolvedIncludeKind::Compiled, settings, session, resolved)) {
            return true;
        }
        if (try_file_variant(index_logical.string() + ".pbt", index_logical, ResolvedIncludeKind::Source, settings, session, resolved)) {
            return true;
        }
        if (try_file_variant(index_logical, index_logical, ResolvedIncludeKind::Source, settings, session, resolved)) {
            return true;
        }
    }

    return false;
}

bool try_logical_target(const std::filesystem::path& root, const std::string& include_path,
                        const EffectiveSettings& settings,
                        RenderSession& session, ResolvedInclude& resolved) {
    return try_target_path(root / include_path, settings, session, resolved);
}

std::vector<std::filesystem::path> include_roots(const std::string& include_path,
                                                 const std::filesystem::path& current_file,
                                                 const EffectiveSettings& settings) {
    std::vector<std::filesystem::path> roots;
    if (is_explicit_relative(include_path)) {
        if (!current_file.empty()) {
            roots.push_back(current_file.parent_path());
        }
        return roots;
    }

    if (!current_file.empty()) {
        roots.push_back(current_file.parent_path());
    }
    roots.insert(roots.end(), settings.include_paths.begin(), settings.include_paths.end());
    if (!settings.include_path.empty()) {
        roots.push_back(settings.include_path);
    }
    roots.push_back(shared_include_root());
    return roots;
}

IncludeResolver::CacheKey cache_key_for(const std::string& include_path,
                                        const std::filesystem::path& current_file,
                                        const EffectiveSettings& settings) {
    IncludeResolver::CacheKey key;
    key.settings = &settings;
    key.current_file = canonical_path(current_file);
    key.include_path = include_path;
    return key;
}

}

ResolvedInclude IncludeResolver::load(const std::string& include_path, const std::filesystem::path& current_file,
                                      const EffectiveSettings& settings, RenderSession& session) const {
    ResolvedInclude resolved;
    const CacheKey cache_key = cache_key_for(include_path, current_file, settings);

    {
        std::lock_guard lock(cache_mutex_);
        auto it = cache_.find(cache_key);
        if (it != cache_.end()) {
            const CacheEntry cached = it->second;
            if (cached.kind == ResolvedIncludeKind::Compiled && cached.compiled_program != nullptr
                && std::chrono::steady_clock::now() < cached.valid_until) {
                if (try_accept_include(cached.physical_path, cached.logical_path, cached.kind, session, resolved)) {
                    resolved.compiled_program = cached.compiled_program;
                    return resolved;
                }
            } else if (try_file_variant(cached.physical_path, cached.logical_path, cached.kind, settings, session, resolved)) {
                return resolved;
            }
            cache_.erase(it);
        }
    }

    if (std::filesystem::path(include_path).is_absolute()) {
        if (try_target_path(std::filesystem::path(include_path).lexically_normal(), settings, session, resolved)) {
            std::lock_guard lock(cache_mutex_);
            cache_[cache_key] = CacheEntry{resolved.path, resolved.logical_path, resolved.kind, resolved.compiled_program,
                                           std::chrono::steady_clock::now() + FileMetadataCache::ttl()};
            return resolved;
        }
    } else {
        for (const std::filesystem::path& root : include_roots(include_path, current_file, settings)) {
            if (try_logical_target(root, include_path, settings, session, resolved)) {
                std::lock_guard lock(cache_mutex_);
                cache_[cache_key] = CacheEntry{resolved.path, resolved.logical_path, resolved.kind, resolved.compiled_program,
                                               std::chrono::steady_clock::now() + FileMetadataCache::ttl()};
                return resolved;
            }
        }
    }

    throw DiagnosticError(make_include_error("Include not found: " + include_path, include_path, session));
}

void IncludeResolver::pop(RenderSession& session) const {
    session.pop_include();
}

}
