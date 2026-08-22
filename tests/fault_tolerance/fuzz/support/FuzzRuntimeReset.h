#pragma once

#include "config/VariableDefinitionParser.h"
#include "runtime/compiled/CompiledTemplateCache.h"
#include "runtime/cache/FileMetadataCache.h"

inline void fuzz_reset_runtime_state() {
    prebyte::FileMetadataCache::instance().clear();
    prebyte::VariableDefinitionParser::clear_import_cache();
    prebyte::CompiledTemplateCache::instance().clear();
}
