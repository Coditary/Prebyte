#pragma once

#include <string>

#include "support/SourceSpan.h"

namespace prebyte {

class Diagnostic;

Diagnostic make_lexer_error(const std::string& message, const std::string& file_path, SourceLocation location);

void trim_right_ascii_whitespace(std::string& text);
void trim_left_ascii_whitespace(std::string& text);

}
