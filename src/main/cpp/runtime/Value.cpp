#include "runtime/Value.h"

#include "support/TextUtil.h"

#include <cmath>
#include <sstream>

namespace prebyte {

namespace {

bool string_is_falsey(const std::string& value) {
    const std::string normalized = text::to_lower(text::trim(value));
    return normalized.empty() || normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off";
}

}

Value::Value(bool value) : storage_(value) {}
Value::Value(double value) : storage_(value) {}
Value::Value(std::string value) : storage_(std::move(value)) {}

bool Value::is_null() const {
    return std::holds_alternative<std::monostate>(storage_);
}

bool Value::to_bool() const {
    if (std::holds_alternative<bool>(storage_)) {
        return std::get<bool>(storage_);
    }
    if (std::holds_alternative<double>(storage_)) {
        return std::fabs(std::get<double>(storage_)) > 0.0;
    }
    if (std::holds_alternative<std::string>(storage_)) {
        return !string_is_falsey(std::get<std::string>(storage_));
    }
    return false;
}

std::string Value::to_string() const {
    if (std::holds_alternative<bool>(storage_)) {
        return std::get<bool>(storage_) ? "true" : "false";
    }
    if (std::holds_alternative<double>(storage_)) {
        std::ostringstream stream;
        stream << std::get<double>(storage_);
        return stream.str();
    }
    if (std::holds_alternative<std::string>(storage_)) {
        return std::get<std::string>(storage_);
    }
    return "";
}

}
