#pragma once

#include <filesystem>
#include <string>

#include "config/ConfigTypes.h"
#include "io/InputBuffer.h"
#include "runtime/RenderSession.h"

namespace prebyte {

struct ResolvedInclude {
    std::filesystem::path path;
    InputBuffer source;
};

class IncludeResolver {
public:
    ResolvedInclude load(const std::string& include_path, const std::filesystem::path& current_file,
                         const EffectiveSettings& settings, RenderSession& session) const;
    void pop(RenderSession& session) const;
};

}
