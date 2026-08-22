#include "config/ConfigTypes.h"
#include "support/FuzzRuntimeReset.h"
#include "runtime/resolution/IncludeResolver.h"
#include "runtime/core/RenderSession.h"
#include "support/Diagnostic.h"
#include "support/FuzzTempDir.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <fuzzer/FuzzedDataProvider.h>
#include <string>

namespace {

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::binary);
    file << content;
}

void seed_filesystem(const std::filesystem::path& root) {
    write_file(root / "main.txt", "{{ include \"header.txt\" }}\n");
    write_file(root / "header.txt", "Header {{ name }}\n");
    write_file(root / "partial.md", "Partial content\n");
    write_file(root / "nested" / "child.txt", "Child {{ name }}\n");
    write_file(root / "nested" / "index.pbt", "Index {{ name }}\n");
    write_file(root / "cycle_a.txt", "{{ include \"cycle_b.txt\" }}");
    write_file(root / "cycle_b.txt", "{{ include \"cycle_a.txt\" }}");
    write_file(root / "escape.txt", "{{ include \"../../../etc/passwd\" }}");
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    FuzzedDataProvider provider(data, size);

    fuzz_reset_runtime_state();

    const std::string include_path = provider.ConsumeRandomLengthString(128);
    const std::string current_file = provider.ConsumeRandomLengthString(128);
    const std::string extra_name = provider.ConsumeRandomLengthString(64);
    const std::string extra_content = provider.ConsumeRandomLengthString(512);
    const std::size_t max_include_depth = provider.ConsumeIntegralInRange<std::size_t>(0, 8);

    FuzzTempDir temp_dir;
    const std::filesystem::path root = temp_dir.path();
    seed_filesystem(root);

    if (!extra_name.empty()) {
        write_file(root / extra_name, extra_content);
    }

    prebyte::EffectiveSettings settings;
    settings.allow_includes = true;
    settings.max_include_depth = max_include_depth;
    settings.include_paths.push_back(root);
    settings.include_paths.push_back(root / "nested");
    settings.include_paths.push_back(root / "alt");

    const std::filesystem::path current_path =
        current_file.empty() ? root / "main.txt" : root / current_file;

    prebyte::IncludeResolver resolver;
    prebyte::RenderSession session;

    try {
        prebyte::ResolvedInclude resolved = resolver.load(include_path, current_path, settings, session);
        if (resolved.kind == prebyte::ResolvedIncludeKind::Source) {
            (void)resolved.source.view();
        }
        resolver.pop(session);
    } catch (const prebyte::DiagnosticError&) {
    } catch (const std::exception&) {
    }

    return 0;
}
