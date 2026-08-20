#include "config/ConfigTypes.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"

#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: generate_fuzz_pbc_seed <output.pbc>\n";
        return 1;
    }

    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    const prebyte::CompiledProgram program = compiler.compile_source(
        "Hello {{ name }}\n{{ if enabled }}Yes{{ else }}No{{ endif }}\n",
        "seed.pbt",
        "seed.pbt",
        settings);

    prebyte::CompiledTemplateSerializer serializer;
    const std::string bytes = serializer.serialize(program);

    std::ofstream output(argv[1], std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return output.good() ? 0 : 1;
}
