#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>

namespace prebyte {

struct FileMetadata {
    bool exists = false;
    std::int64_t mtime_ticks = 0;
};

class FileMetadataCache {
public:
    static FileMetadataCache& instance();
    static std::chrono::steady_clock::duration ttl();

    FileMetadata probe(const std::filesystem::path& path);
    void remember(const std::filesystem::path& path, FileMetadata metadata);
    void invalidate(const std::filesystem::path& path);

private:
    struct Entry {
        FileMetadata metadata;
        std::chrono::steady_clock::time_point expires_at;
    };

    FileMetadataCache() = default;

    std::filesystem::path normalize_path(const std::filesystem::path& path) const;
    FileMetadata stat_path(const std::filesystem::path& path) const;

    std::mutex mutex_;
    std::map<std::filesystem::path, Entry> cache_;
};

}
