#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace prebyte {

class OutputWriter {
public:
    void write(std::string_view output, const std::optional<std::filesystem::path>& output_path,
               std::string_view encoding = "utf-8") const;
};

}
