#include "parser/FileParser.h"
#include "support/FuzzTempDir.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <fuzzer/FuzzedDataProvider.h>
#include <string>

namespace {

constexpr const char* kExtensions[] = {".json", ".yaml", ".yml", ".ini", ".cfg", ".env", ".toml", ".txt"};

void try_parse_path(const std::string& file_path) {
    try {
        prebyte::FileParser parser;
        (void)parser.parse(file_path);
    } catch (const std::exception&) {
    }
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    file << content;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        try_parse_path("");
        return 0;
    }

    FuzzedDataProvider provider(data, size);
    const int route = provider.ConsumeIntegralInRange(0, 4);

    if (route == 0) {
        try_parse_path("");
        return 0;
    }

    FuzzTempDir temp_dir;
    if (route == 1) {
        try_parse_path((temp_dir.path() / "missing.json").string());
        return 0;
    }

    const int extension_index = provider.ConsumeIntegralInRange(0, 7);
    const std::string content = provider.ConsumeRemainingBytesAsString();
    const std::filesystem::path path =
        temp_dir.path() / ("input" + std::string(kExtensions[extension_index]));
    write_file(path, content);
    try_parse_path(path.string());
    return 0;
}
