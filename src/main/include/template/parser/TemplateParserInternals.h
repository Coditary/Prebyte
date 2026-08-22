#pragma once

#include <cstddef>
#include <string_view>

#include "template/ast/TemplateNode.h"
#include "template/lexer/TemplateToken.h"

namespace prebyte {

constexpr std::size_t kMaxParserExpressionDepth = 64;

bool is_reserved_name(std::string_view name);
bool is_reserved_loop_binding(std::string_view name);

void apply_trim_flags(TemplateNode& node, const TemplateToken& start, const TemplateToken& end);

}
