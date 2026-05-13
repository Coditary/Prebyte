#pragma once

#include <filesystem>
#include <optional>

#include "io/InputBuffer.h"

namespace prebyte {

class InputReader {
public:
    InputBuffer read(const std::optional<std::filesystem::path>& input_path) const;
};

}
