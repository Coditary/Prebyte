#include "runtime/CompiledTemplateSerializer.h"
#include "support/Diagnostic.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(data), size);

    try {
        prebyte::CompiledTemplateSerializer serializer;
        (void)serializer.deserialize(input);
    } catch (const prebyte::DiagnosticError&) {
    } catch (const std::exception&) {
    }

    return 0;
}
