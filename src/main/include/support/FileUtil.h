#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace prebyte::file_util {

std::string read_text_file(const std::filesystem::path& path);
bool write_text_file(const std::filesystem::path& path, std::string_view content);

}
