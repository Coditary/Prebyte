#include "config/SettingsLoader.h"
#include "support/Diagnostic.h"
#include "support/FuzzTempDir.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <fuzzer/FuzzedDataProvider.h>
#include <string>

namespace {

constexpr const char* kSettingsExtensions[] = {".yaml", ".json", ".toml", ".ini"};

std::filesystem::path write_settings_file(const std::filesystem::path& directory, const std::string& extension,
                                            const std::string& content) {
    const std::filesystem::path path = directory / ("settings" + extension);
    std::ofstream file(path, std::ios::binary);
    file << content;
    return path;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    FuzzedDataProvider provider(data, size);
    const int extension_index = provider.ConsumeIntegralInRange(0, 3);
    const std::string content = provider.ConsumeRemainingBytesAsString();

    FuzzTempDir temp_dir;
    const std::filesystem::path path =
        write_settings_file(temp_dir.path(), kSettingsExtensions[extension_index], content);

    try {
        prebyte::SettingsLoader loader;
        (void)loader.load(path);
    } catch (const prebyte::DiagnosticError&) {
    } catch (const std::exception&) {
    }

    return 0;
}
