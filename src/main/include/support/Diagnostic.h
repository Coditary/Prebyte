#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "support/SourceSpan.h"

namespace prebyte {

enum class DiagnosticSeverity {
    Error,
    Warning,
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string code;
    std::string message;
    SourceSpan span;
    std::string snippet;
    std::vector<std::string> include_chain;

    std::string format() const;
};

class DiagnosticError : public std::runtime_error {
public:
    explicit DiagnosticError(Diagnostic diagnostic);

    const Diagnostic& diagnostic() const noexcept;

private:
    Diagnostic diagnostic_;
};

}
