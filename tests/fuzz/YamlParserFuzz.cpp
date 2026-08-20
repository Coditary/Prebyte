#include "parser/YamlParser.h"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input(reinterpret_cast<const char*>(data), size);

    try {
        prebyte::YamlParser parser;
        (void)parser.parse_string(input);
    } catch (const std::exception&) {
    }

    return 0;
}
