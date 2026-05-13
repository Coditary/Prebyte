#include "support/Diagnostic.h"

#include <sstream>

namespace prebyte {

std::string Diagnostic::format() const {
    std::ostringstream stream;
    const char* severity_name = severity == DiagnosticSeverity::Error ? "error" : "warning";
    stream << severity_name << "[" << code << "]: " << message;

    if (!span.file_path.empty()) {
        stream << "\n  --> " << span.file_path << ':' << span.start.line << ':' << span.start.column;
    }

    if (!snippet.empty()) {
        stream << "\n  " << span.start.line << " | " << snippet;
        stream << "\n    | " << std::string(span.start.column > 0 ? span.start.column - 1 : 0, ' ') << '^';
    }

    if (!include_chain.empty()) {
        stream << "\n  include chain: ";
        for (std::size_t index = 0; index < include_chain.size(); ++index) {
            if (index != 0) {
                stream << " -> ";
            }
            stream << include_chain[index];
        }
    }

    return stream.str();
}

DiagnosticError::DiagnosticError(Diagnostic diagnostic)
    : std::runtime_error(diagnostic.format()), diagnostic_(std::move(diagnostic)) {}

const Diagnostic& DiagnosticError::diagnostic() const noexcept {
    return diagnostic_;
}

}
