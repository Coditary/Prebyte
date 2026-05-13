#include "io/InputReader.h"

#include "support/Diagnostic.h"

#include <iostream>
#include <sstream>

namespace prebyte {

InputBuffer InputReader::read(const std::optional<std::filesystem::path>& input_path) const {
    if (!input_path.has_value()) {
        std::ostringstream stream;
        stream << std::cin.rdbuf();
        return InputBuffer::from_owned(stream.str());
    }

    try {
        return InputBuffer::from_file(*input_path);
    } catch (const std::exception&) {
        Diagnostic diagnostic;
        diagnostic.code = "IO001";
        diagnostic.message = "Cannot read input file: " + input_path->string();
        diagnostic.span.file_path = input_path->string();
        throw DiagnosticError(diagnostic);
    }
}

}
