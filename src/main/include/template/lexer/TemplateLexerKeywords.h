#pragma once

#include <optional>
#include <string_view>

#include "template/lexer/TemplateToken.h"

namespace prebyte::template_lexer {

// Maps a scanned identifier lexeme to a keyword/boolean token type, if any.
std::optional<TemplateTokenType> lookup_keyword(std::string_view lexeme);

}
