#include "runtime/FilterRegistry.h"

#include "support/Diagnostic.h"
#include "support/TextUtil.h"

namespace prebyte {

namespace {

Diagnostic make_runtime_error(const std::string& message) {
    Diagnostic diagnostic;
    diagnostic.code = "RUNTIME001";
    diagnostic.message = message;
    return diagnostic;
}

void require_arity(std::string_view name, const std::vector<Value>& arguments, std::size_t expected) {
    if (arguments.size() != expected) {
        throw DiagnosticError(make_runtime_error("Wrong filter arity for " + std::string(name)));
    }
}

std::string scalar_string(std::string_view name, const Value& value) {
    if (value.is_object() || value.is_list()) {
        throw DiagnosticError(make_runtime_error("Filter '" + std::string(name) + "' requires scalar input"));
    }
    return value.to_string();
}

Value apply_trim(const std::vector<Value>& arguments) {
    require_arity("trim", arguments, 1);
    return Value(text::trim(scalar_string("trim", arguments[0])));
}

Value apply_upper(const std::vector<Value>& arguments) {
    require_arity("upper", arguments, 1);
    std::string value = scalar_string("upper", arguments[0]);
    for (char& ch : value) {
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 'a' + 'A');
        }
    }
    return Value(std::move(value));
}

Value apply_lower(const std::vector<Value>& arguments) {
    require_arity("lower", arguments, 1);
    return Value(text::to_lower(scalar_string("lower", arguments[0])));
}

Value apply_default(const std::vector<Value>& arguments) {
    require_arity("default", arguments, 2);
    return arguments[0].is_null() ? arguments[1] : arguments[0];
}

Value apply_replace(const std::vector<Value>& arguments) {
    require_arity("replace", arguments, 3);
    std::string value = scalar_string("replace", arguments[0]);
    const std::string from = scalar_string("replace", arguments[1]);
    const std::string to = scalar_string("replace", arguments[2]);
    if (from.empty()) {
        return Value(std::move(value));
    }
    std::size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
    return Value(std::move(value));
}

}

Value FilterRegistry::apply(std::string_view name, const std::vector<Value>& arguments) const {
    if (name == "trim") {
        return apply_trim(arguments);
    }
    if (name == "upper") {
        return apply_upper(arguments);
    }
    if (name == "lower") {
        return apply_lower(arguments);
    }
    if (name == "default") {
        return apply_default(arguments);
    }
    if (name == "replace") {
        return apply_replace(arguments);
    }
    throw DiagnosticError(make_runtime_error("Unknown filter: " + std::string(name)));
}

}
