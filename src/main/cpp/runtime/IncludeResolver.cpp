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

constexpr std::size_t kMaxIncludePathLength = 4096;
constexpr std::size_t kMaxIncludePathSeparators = 64;

void validate_include_path(const std::string& include_path, const RenderSession& session) {
    if (include_path.size() > kMaxIncludePathLength) {
        throw DiagnosticError(make_include_error("Include path is too long", include_path, session));
    }

    if (std::filesystem::path(include_path).is_absolute()) {
        throw DiagnosticError(make_include_error("Absolute include paths are not allowed", include_path, session));
    }

    std::size_t separators = 0;
    for (char ch : include_path) {
        if (ch == '/' || ch == '\\') {
            ++separators;
        }
    }
    if (separators > kMaxIncludePathSeparators) {
        throw DiagnosticError(make_include_error("Include path is too deep", include_path, session));
    }
}

bool is_path_within_root(const std::filesystem::path& path, const std::filesystem::path& root) {
    if (root.empty() || path.empty()) {
        return false;
    }

    std::error_code error;
    std::filesystem::path resolved_path = path;
    std::filesystem::path resolved_root = root;
    if (path_exists(path) || path.is_absolute()) {
        const std::filesystem::path canonical_path = std::filesystem::weakly_canonical(path, error);
        if (!error) {
            resolved_path = canonical_path;
        }
    } else {
        resolved_path = canonical_path(path);
    }
    if (path_exists(root) || root.is_absolute()) {
        const std::filesystem::path canonical_root = std::filesystem::weakly_canonical(root, error);
        if (!error) {
            resolved_root = canonical_root;
        }
    } else {
        resolved_root = canonical_path(root);
    }

    const std::filesystem::path relative = resolved_path.lexically_relative(resolved_root);
    if (relative.empty()) {
        return resolved_path == resolved_root;
    }
    if (relative == ".") {
        return true;
    }

    const std::string relative_generic = relative.generic_string();
    return !(relative_generic == ".." || relative_generic.starts_with("../") || relative_generic.contains("/../"));
}

std::vector<std::filesystem::path> trusted_include_roots(const std::filesystem::path& current_file,
                                                         const EffectiveSettings& settings,
                                                         const RenderSession& session) {
    std::vector<std::filesystem::path> roots;
    auto add_root = [&](const std::filesystem::path& path) {
        if (!path.empty()) {
            roots.push_back(canonical_path(path));
        }
    };

    if (session.include_anchor_root.has_value()) {
        add_root(*session.include_anchor_root);
    } else if (!current_file.empty()) {
        add_root(current_file.parent_path());
    }
    for (const std::filesystem::path& path : settings.include_paths) {
        add_root(path);
    }
    add_root(settings.include_path);
    add_root(shared_include_root());
    return roots;
}

void validate_resolved_include_target(const std::filesystem::path& physical_path,
                                      const std::filesystem::path& current_file,
                                      const EffectiveSettings& settings,
                                      const RenderSession& session) {
    const std::filesystem::path absolute_target = canonical_path(physical_path);
    for (const std::filesystem::path& root : trusted_include_roots(current_file, settings, session)) {
        if (is_path_within_root(absolute_target, root)) {
            return;
        }
    }
    throw DiagnosticError(make_include_error("Include path escapes allowed roots", physical_path, session));
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
                      ResolvedIncludeKind kind, const std::filesystem::path& current_file,
                      const EffectiveSettings& settings,
                      RenderSession& session, ResolvedInclude& resolved) {
    validate_resolved_include_target(physical_path, current_file, settings, session);

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

bool try_target_path(const std::filesystem::path& logical, const std::filesystem::path& current_file,
                     const EffectiveSettings& settings,
                     RenderSession& session, ResolvedInclude& resolved) {
    const std::filesystem::path pbc = logical.string() + ".pbc";
    if (try_file_variant(pbc, logical, ResolvedIncludeKind::Compiled, current_file, settings, session, resolved)) {
        return true;
    }

    const std::filesystem::path pbt = logical.string() + ".pbt";
    if (try_file_variant(pbt, logical, ResolvedIncludeKind::Source, current_file, settings, session, resolved)) {
        return true;
    }

    if (try_file_variant(logical, logical, ResolvedIncludeKind::Source, current_file, settings, session, resolved)) {
        return true;
    }

    if (path_is_directory(logical)) {
        const std::filesystem::path index_logical = logical / "index";
        if (try_file_variant(index_logical.string() + ".pbc", index_logical, ResolvedIncludeKind::Compiled, current_file, settings, session, resolved)) {
            return true;
        }
        if (try_file_variant(index_logical.string() + ".pbt", index_logical, ResolvedIncludeKind::Source, current_file, settings, session, resolved)) {
            return true;
        }
        if (try_file_variant(index_logical, index_logical, ResolvedIncludeKind::Source, current_file, settings, session, resolved)) {
            return true;
        }
    }

    return false;
}

bool try_logical_target(const std::filesystem::path& root, const std::string& include_path,
                        const std::filesystem::path& current_file,
                        const EffectiveSettings& settings,
                        RenderSession& session, ResolvedInclude& resolved) {
    return try_target_path(root / include_path, current_file, settings, session, resolved);
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
    validate_include_path(include_path, session);

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
            } else if (try_file_variant(cached.physical_path, cached.logical_path, cached.kind, current_file, settings, session, resolved)) {
                return resolved;
            }
            cache_.erase(it);
        }
    }

    if (std::filesystem::path(include_path).is_absolute()) {
        throw DiagnosticError(make_include_error("Absolute include paths are not allowed", include_path, session));
    } else {
        for (const std::filesystem::path& root : include_roots(include_path, current_file, settings)) {
            if (try_logical_target(root, include_path, current_file, settings, session, resolved)) {
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
