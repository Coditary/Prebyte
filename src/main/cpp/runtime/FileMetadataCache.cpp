#include "runtime/FileMetadataCache.h"

#include <limits>

#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <sys/stat.h>
#endif

namespace prebyte {

namespace {

constexpr auto kMetadataCacheTtl = std::chrono::milliseconds(250);

const std::filesystem::path& current_working_directory() {
    static const std::filesystem::path cwd = []() {
        std::error_code error;
        const std::filesystem::path path = std::filesystem::current_path(error);
        return error ? std::filesystem::path{} : path;
    }();
    return cwd;
}

}

FileMetadataCache& FileMetadataCache::instance() {
    static FileMetadataCache cache;
    return cache;
}

std::chrono::steady_clock::duration FileMetadataCache::ttl() {
    return kMetadataCacheTtl;
}

FileMetadata FileMetadataCache::probe(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }

    const std::filesystem::path normalized = normalize_path(path);
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard lock(mutex_);
        auto it = cache_.find(normalized);
        if (it != cache_.end() && now < it->second.expires_at) {
            return it->second.metadata;
        }
    }

    const FileMetadata metadata = stat_path(normalized);
    remember(normalized, metadata);
    return metadata;
}

void FileMetadataCache::remember(const std::filesystem::path& path, FileMetadata metadata) {
    if (path.empty()) {
        return;
    }

    std::lock_guard lock(mutex_);
    cache_[normalize_path(path)] = Entry{metadata, std::chrono::steady_clock::now() + kMetadataCacheTtl};
}

void FileMetadataCache::invalidate(const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }

    std::lock_guard lock(mutex_);
    cache_.erase(normalize_path(path));
}

std::filesystem::path FileMetadataCache::normalize_path(const std::filesystem::path& path) const {
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

FileMetadata FileMetadataCache::stat_path(const std::filesystem::path& path) const {
#if defined(__unix__) || defined(__APPLE__)
    struct stat info {};
    if (::stat(path.c_str(), &info) == 0) {
        return FileMetadata{
            .exists = true,
            .mtime_ticks = static_cast<std::int64_t>(info.st_mtim.tv_sec) * 1000000000LL
                + static_cast<std::int64_t>(info.st_mtim.tv_nsec),
        };
    }

    if (errno == ENOENT || errno == ENOTDIR) {
        return {};
    }
    return FileMetadata{.exists = false, .mtime_ticks = std::numeric_limits<std::int64_t>::max()};
#else
    std::error_code error;
    const auto time = std::filesystem::last_write_time(path, error);
    if (error) {
        return FileMetadata{.exists = false, .mtime_ticks = std::numeric_limits<std::int64_t>::max()};
    }
    return FileMetadata{
        .exists = true,
        .mtime_ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count(),
    };
#endif
}

}
