#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>

class FuzzTempDir {
public:
    FuzzTempDir() {
        path_ = root_directory() / std::to_string(next_sequence());
        std::filesystem::create_directories(path_);
    }

    ~FuzzTempDir() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    FuzzTempDir(const FuzzTempDir&) = delete;
    FuzzTempDir& operator=(const FuzzTempDir&) = delete;

    const std::filesystem::path& path() const {
        return path_;
    }

private:
    static std::filesystem::path root_directory() {
        static const std::filesystem::path root = []() {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            const std::filesystem::path path = std::filesystem::temp_directory_path()
                / ("prebyte-fuzz-root-" + std::to_string(static_cast<unsigned long long>(stamp)));
            std::filesystem::create_directories(path);
            return path;
        }();
        return root;
    }

    static std::uint64_t next_sequence() {
        static std::atomic<std::uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }

    std::filesystem::path path_;
};
