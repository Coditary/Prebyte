#pragma once

#include "config/VariableDefinitionParser.h"
#include "runtime/CompiledTemplateCache.h"
#include "runtime/FileMetadataCache.h"

inline void fuzz_reset_runtime_state() {
    prebyte::FileMetadataCache::instance().clear();
    prebyte::VariableDefinitionParser::clear_import_cache();
    prebyte::CompiledTemplateCache::instance().clear();
}
