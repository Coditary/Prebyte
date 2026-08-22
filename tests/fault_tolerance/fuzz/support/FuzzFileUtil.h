#pragma once

#include "support/FileUtil.h"

#include <filesystem>
#include <string>

inline void fuzz_write_file(const std::filesystem::path& path, const std::string& content) {
    prebyte::file_util::write_text_file(path, content);
}
