#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "runtime/RenderSession.h"
#include "support/SourceSpan.h"

namespace prebyte {

class BuiltinRegistry {
public:
    std::optional<std::string> resolve(const std::string& name, const SourceSpan& span,
                                       const std::filesystem::path& current_file,
                                       const RenderSession& session) const;
};

}
