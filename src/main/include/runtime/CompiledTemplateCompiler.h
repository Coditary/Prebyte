#pragma once

#include <filesystem>
#include <string_view>

#include "config/ConfigTypes.h"
#include "runtime/CompiledTemplateProgram.h"
#include "template/ast/TemplateNode.h"

namespace prebyte {

class CompiledTemplateCompiler {
public:
    CompiledProgram compile_source(std::string_view source, const std::filesystem::path& source_path,
                                   const std::filesystem::path& logical_path, const EffectiveSettings& settings) const;

private:
    CompiledProgram compile_document(const DocumentNode& document, const std::filesystem::path& source_path,
                                     const std::filesystem::path& logical_path, const EffectiveSettings& settings) const;
};

}
