#pragma once

#include <chrono>
#include <filesystem>
#include <string>

class FuzzTempDir {
public:
    FuzzTempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path()
            / ("prebyte-fuzz-" + std::to_string(static_cast<unsigned long long>(stamp)));
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
    std::filesystem::path path_;
};
