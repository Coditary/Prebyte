#include "template/parser/TemplateParserInternals.h"

namespace prebyte {

namespace {

constexpr std::string_view kBuiltinNames[] = {
    "__TIME__",
    "__LINE__",
    "__FILE__",
    "__FILENAME__",
    "__DIR__",
    "__EXTENSION__",
    "__DATE__",
    "__TIMESTAMP__",
    "__YEAR__",
    "__MONTH__",
    "__DAY__",
    "__UNIX_EPOCH__",
    "__USER__",
    "__HOST__",
    "__OS__",
    "__WORKING_DIR__",
    "__UUID__",
    "__RANDOM__",
};

constexpr std::string_view kKeywordNames[] = {
    "if",
    "elseif",
    "else",
    "endif",
    "for",
    "in",
    "endfor",
    "include",
    "set",
    "lua",
    "fn",
    "endfn",
    "endlua",
    "len",
};

bool is_builtin_name(std::string_view name) {
    for (const std::string_view builtin : kBuiltinNames) {
        if (builtin == name) {
            return true;
        }
    }
    return false;
}

bool is_keyword_name(std::string_view name) {
    for (const std::string_view keyword : kKeywordNames) {
        if (keyword == name) {
            return true;
        }
    }
    return false;
}

} // namespace

bool is_reserved_name(std::string_view name) {
    return name == "loop" || name == "ARGS" || is_builtin_name(name) || is_keyword_name(name);
}

bool is_reserved_loop_binding(std::string_view name) {
    return is_reserved_name(name);
}

void apply_trim_flags(TemplateNode& node, const TemplateToken& start, const TemplateToken& end) {
    node.trim_left = start.trim_left;
    node.trim_right = end.trim_right;
}

}
