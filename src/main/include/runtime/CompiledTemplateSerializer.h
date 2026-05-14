#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "config/ConfigTypes.h"
#include "runtime/FileMetadataCache.h"
#include "runtime/CompiledTemplateProgram.h"

namespace prebyte {

class CompiledTemplateSerializer {
public:
    std::string serialize(const CompiledProgram& program) const;
    CompiledProgram deserialize(std::string_view bytes, const std::filesystem::path& compiled_path = {}) const;
    const CompiledProgram* try_load_valid(const std::filesystem::path& path,
                                          const EffectiveSettings& settings) const;
    bool is_fresh(const CompiledProgram& program, const EffectiveSettings& settings) const;

    std::filesystem::path compiled_path_for_source(const std::filesystem::path& source_path) const;
    std::filesystem::path logical_path_for_source(const std::filesystem::path& source_path) const;

private:
    std::int64_t mtime_ticks(const std::filesystem::path& path) const;
};

}
