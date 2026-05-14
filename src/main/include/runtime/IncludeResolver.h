#pragma once

#include <filesystem>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "config/ConfigTypes.h"
#include "io/InputBuffer.h"
#include "runtime/CompiledTemplateProgram.h"
#include "runtime/RenderSession.h"

namespace prebyte {

enum class ResolvedIncludeKind {
    Compiled,
    Source,
};

struct ResolvedInclude {
    std::filesystem::path path;
    std::filesystem::path logical_path;
    ResolvedIncludeKind kind = ResolvedIncludeKind::Source;
    const CompiledProgram* compiled_program = nullptr;
    InputBuffer source;
};

class IncludeResolver {
public:
    ResolvedInclude load(const std::string& include_path, const std::filesystem::path& current_file,
                         const EffectiveSettings& settings, RenderSession& session) const;
    void pop(RenderSession& session) const;

    struct CacheKey {
        const EffectiveSettings* settings = nullptr;
        std::filesystem::path current_file;
        std::string include_path;

        auto operator<=>(const CacheKey&) const = default;
    };

    struct CacheEntry {
        std::filesystem::path physical_path;
        std::filesystem::path logical_path;
        ResolvedIncludeKind kind = ResolvedIncludeKind::Source;
        const CompiledProgram* compiled_program = nullptr;
        std::chrono::steady_clock::time_point valid_until = std::chrono::steady_clock::time_point::min();
    };

private:

    mutable std::mutex cache_mutex_;
    mutable std::map<CacheKey, CacheEntry> cache_;
};

}
