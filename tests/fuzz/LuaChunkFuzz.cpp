#include "config/ConfigTypes.h"
#include "runtime/LuaRuntime.h"
#include "support/Diagnostic.h"
#include "support/FuzzTempDir.h"
#include "support/SourceSpan.h"

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <fstream>
#include <fuzzer/FuzzedDataProvider.h>
#include <limits>
#include <string>

namespace {

prebyte::LuaChunkMode chunk_mode_for_index(int index) {
    switch (index) {
    case 0:
        return prebyte::LuaChunkMode::InlineValue;
    case 1:
        return prebyte::LuaChunkMode::Predicate;
    case 2:
    default:
        return prebyte::LuaChunkMode::BlockValue;
    }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        return 0;
    }

    FuzzedDataProvider provider(data, size);
    const int mode_index = provider.ConsumeIntegralInRange(0, 2);
    prebyte::EffectiveSettings settings;
    settings.lua_instruction_limit = provider.ConsumeIntegralInRange(1000, 100000);
    settings.lua_memory_limit_bytes = provider.ConsumeIntegralInRange(512 * 1024, 4 * 1024 * 1024);
    settings.max_render_time_ms = provider.ConsumeBool() ? 0 : std::numeric_limits<std::size_t>::max();
    const bool seed_session = provider.ConsumeBool();
    const std::string source = provider.ConsumeRemainingBytesAsString();

    if (source.empty()) {
        return 0;
    }

    prebyte::RenderSession session;
    if (seed_session) {
        session.variables.set("name", "Ada");
        session.variables.set_value("count", prebyte::Value(2.0));
        session.args = {"alpha", "beta"};
        if (settings.max_render_time_ms == 0) {
            session.start_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
        }
    }

    prebyte::SourceSpan span;
    span.file_path = "fuzz.lua";
    span.start.line = 1;

    try {
        prebyte::LuaRuntime runtime;
        (void)runtime.execute(source, chunk_mode_for_index(mode_index), settings, session, "fuzz.lua", span);
    } catch (const prebyte::DiagnosticError&) {
    } catch (const std::exception&) {
    }

    return 0;
}
