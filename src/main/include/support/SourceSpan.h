#pragma once

#include <cstddef>
#include <string>

namespace prebyte {

struct SourceLocation {
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

struct SourceSpan {
    std::string file_path;
    SourceLocation start;
    SourceLocation end;
};

}
