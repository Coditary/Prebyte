#include "runtime/Value.h"

#include "support/TextUtil.h"

#include <cmath>
#include <compare>
#include <cstdlib>
#include <format>
#include <optional>
#include <sstream>

namespace prebyte {

namespace {

Value value_from_data(const Data& data) {
    if (data.is_null()) {
        return Value();
    }
    if (data.is_bool()) {
        return Value(data.as_bool());
    }
    if (data.is_int()) {
        return Value(static_cast<double>(data.as_int()));
    }
    if (data.is_double()) {
        return Value(data.as_double());
    }
    if (data.is_string()) {
        return Value(data.as_string());
    }
    if (data.is_map()) {
        return Value::object(data.as_map());
    }
    if (data.is_array()) {
        return Value::list(data.as_array());
    }
    return Value();
}

bool equals_lower_ascii(std::string_view value, std::string_view expected) {
    if (value.size() != expected.size()) {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        char current = value[index];
        if (current >= 'A' && current <= 'Z') {
            current = static_cast<char>(current - 'A' + 'a');
        }
        if (current != expected[index]) {
            return false;
        }
    }
    return true;
}

bool string_is_falsey(std::string_view value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    value = value.substr(start, end - start);
    return value.empty() || equals_lower_ascii(value, "false") || value == "0"
        || equals_lower_ascii(value, "no") || equals_lower_ascii(value, "off");
}

std::optional<double> parse_scalar_number(std::string_view value) {
    std::string text(value);
    text = text::trim(std::move(text));
    if (text.empty()) {
        return std::nullopt;
    }
    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (end == nullptr || *end != '\0') {
        return std::nullopt;
    }
    return parsed;
}

}

Value::Value(bool value) : storage_(value) {}
Value::Value(double value) : storage_(value) {}
Value::Value(std::string value) : storage_(std::move(value)) {}
Value::Value(Storage storage) : storage_(std::move(storage)) {}

Value Value::borrowed(std::string_view value) {
    return Value(Storage(value));
}

Value Value::borrowed_data(const Data& data) {
    return Value(Storage(BorrowedData{.data = &data}));
}

Value Value::object(Object value) {
    return Value(Storage(std::move(value)));
}

Value Value::list(List value) {
    return Value(Storage(std::move(value)));
}

bool Value::is_null() const {
    return std::holds_alternative<std::monostate>(storage_);
}

bool Value::is_object() const {
    return std::holds_alternative<Object>(storage_)
        || (std::holds_alternative<BorrowedData>(storage_) && std::get<BorrowedData>(storage_).data != nullptr
            && std::get<BorrowedData>(storage_).data->is_map());
}

bool Value::is_list() const {
    return std::holds_alternative<List>(storage_)
        || (std::holds_alternative<BorrowedData>(storage_) && std::get<BorrowedData>(storage_).data != nullptr
             && std::get<BorrowedData>(storage_).data->is_array());
}

std::optional<bool> Value::try_as_bool() const {
    if (std::holds_alternative<bool>(storage_)) {
        return std::get<bool>(storage_);
    }
    if (std::holds_alternative<BorrowedData>(storage_)) {
        const Data* data = std::get<BorrowedData>(storage_).data;
        if (data != nullptr && data->is_bool()) {
            return data->as_bool();
        }
    }
    return std::nullopt;
}

std::optional<double> Value::try_as_number() const {
    if (std::holds_alternative<double>(storage_)) {
        return std::get<double>(storage_);
    }
    if (std::holds_alternative<BorrowedData>(storage_)) {
        const Data* data = std::get<BorrowedData>(storage_).data;
        if (data != nullptr) {
            if (data->is_int()) {
                return static_cast<double>(data->as_int());
            }
            if (data->is_double()) {
                return data->as_double();
            }
        }
    }
    return std::nullopt;
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
    if (std::holds_alternative<std::string_view>(storage_)) {
        return !string_is_falsey(std::get<std::string_view>(storage_));
    }
    if (std::holds_alternative<Object>(storage_)) {
        return !std::get<Object>(storage_).empty();
    }
    if (std::holds_alternative<List>(storage_)) {
        return !std::get<List>(storage_).empty();
    }
    if (std::holds_alternative<BorrowedData>(storage_)) {
        const Data* data = std::get<BorrowedData>(storage_).data;
        if (data == nullptr) {
            return false;
        }
        if (data->is_null()) {
            return false;
        }
        if (data->is_bool()) {
            return data->as_bool();
        }
        if (data->is_int()) {
            return data->as_int() != 0;
        }
        if (data->is_double()) {
            return std::fabs(data->as_double()) > 0.0;
        }
        if (data->is_string()) {
            return !string_is_falsey(data->as_string_ref());
        }
        if (data->is_map()) {
            return !data->as_map().empty();
        }
        if (data->is_array()) {
            return !data->as_array().empty();
        }
    }
    return false;
}

std::size_t Value::length() const {
    if (const auto value = try_as_string_view()) {
        return value->size();
    }
    if (std::holds_alternative<Object>(storage_)) {
        return std::get<Object>(storage_).size();
    }
    if (std::holds_alternative<List>(storage_)) {
        return std::get<List>(storage_).size();
    }
    if (std::holds_alternative<BorrowedData>(storage_)) {
        const Data* data = std::get<BorrowedData>(storage_).data;
        if (data == nullptr) {
            return 0;
        }
        if (data->is_map()) {
            return data->as_map().size();
        }
        if (data->is_array()) {
            return data->as_array().size();
        }
    }
    return 0;
}

std::optional<std::string_view> Value::try_as_string_view() const {
    if (std::holds_alternative<std::string>(storage_)) {
        return std::get<std::string>(storage_);
    }
    if (std::holds_alternative<std::string_view>(storage_)) {
        return std::get<std::string_view>(storage_);
    }
    if (std::holds_alternative<BorrowedData>(storage_)) {
        const Data* data = std::get<BorrowedData>(storage_).data;
        if (data != nullptr && data->is_string()) {
            return std::string_view(data->as_string_ref());
        }
    }
    return std::nullopt;
}

std::string_view Value::as_string_view() const {
    if (const auto value = try_as_string_view()) {
        return *value;
    }
    return {};
}

const Value::Object* Value::try_as_object() const {
    if (std::holds_alternative<Object>(storage_)) {
        return &std::get<Object>(storage_);
    }
    if (std::holds_alternative<BorrowedData>(storage_)) {
        const Data* data = std::get<BorrowedData>(storage_).data;
        if (data != nullptr && data->is_map()) {
            return &data->as_map();
        }
    }
    return nullptr;
}

const Value::List* Value::try_as_list() const {
    if (std::holds_alternative<List>(storage_)) {
        return &std::get<List>(storage_);
    }
    if (std::holds_alternative<BorrowedData>(storage_)) {
        const Data* data = std::get<BorrowedData>(storage_).data;
        if (data != nullptr && data->is_array()) {
            return &data->as_array();
        }
    }
    return nullptr;
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
    if (std::holds_alternative<std::string_view>(storage_)) {
        return std::string(std::get<std::string_view>(storage_));
    }
    if (std::holds_alternative<BorrowedData>(storage_)) {
        const Data* data = std::get<BorrowedData>(storage_).data;
        if (data == nullptr) {
            return "";
        }
        if (data->is_null()) {
            return "";
        }
        if (data->is_bool()) {
            return data->as_bool() ? "true" : "false";
        }
        if (data->is_int()) {
            return std::to_string(data->as_int());
        }
        if (data->is_double()) {
            std::ostringstream stream;
            stream << data->as_double();
            return stream.str();
        }
        if (data->is_string()) {
            return data->as_string_ref();
        }
        return "";
    }
    if (std::holds_alternative<Object>(storage_) || std::holds_alternative<List>(storage_)) {
        return "";
    }
    return "";
}

void Value::append_to(std::string& output) const {
    if (std::holds_alternative<std::string>(storage_)) {
        const std::string& value = std::get<std::string>(storage_);
        output.append(value.data(), value.size());
        return;
    }
    if (std::holds_alternative<std::string_view>(storage_)) {
        const std::string_view value = std::get<std::string_view>(storage_);
        output.append(value.data(), value.size());
        return;
    }
    if (std::holds_alternative<bool>(storage_)) {
        output.append(std::get<bool>(storage_) ? "true" : "false");
        return;
    }
    if (std::holds_alternative<double>(storage_)) {
        output += std::format("{}", std::get<double>(storage_));
        return;
    }
    if (std::holds_alternative<BorrowedData>(storage_)) {
        output += to_string();
    }
}

bool Value::equals(const Value& other) const {
    if (is_object() || is_list() || other.is_object() || other.is_list()) {
        return false;
    }
    const std::string_view left = as_string_view();
    const std::string_view right = other.as_string_view();
    if ((!left.empty() || std::holds_alternative<std::string>(storage_) || std::holds_alternative<std::string_view>(storage_))
        && (!right.empty() || std::holds_alternative<std::string>(other.storage_) || std::holds_alternative<std::string_view>(other.storage_))) {
        return left == right;
    }
    return to_string() == other.to_string();
}

std::optional<std::strong_ordering> Value::compare_scalar(const Value& other) const {
    if (is_object() || is_list() || other.is_object() || other.is_list()) {
        return std::nullopt;
    }

    const std::optional<double> left_number = parse_scalar_number(to_string());
    const std::optional<double> right_number = parse_scalar_number(other.to_string());
    if (left_number.has_value() && right_number.has_value()) {
        if (*left_number < *right_number) {
            return std::strong_ordering::less;
        }
        if (*left_number > *right_number) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }

    const std::string left = to_string();
    const std::string right = other.to_string();
    if (left < right) {
        return std::strong_ordering::less;
    }
    if (left > right) {
        return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
}

std::optional<Value> Value::member(std::string_view key) const {
    const Object* object = try_as_object();
    if (object == nullptr) {
        return std::nullopt;
    }

    auto it = object->find(std::string(key));
    if (it == object->end()) {
        return std::nullopt;
    }
    return from_data(it->second);
}

std::optional<Value> Value::index(std::size_t index) const {
    const List* list = try_as_list();
    if (list == nullptr) {
        return std::nullopt;
    }

    if (index >= list->size()) {
        return std::nullopt;
    }
    return from_data((*list)[index]);
}

std::vector<Value> Value::list_items() const {
    std::vector<Value> items;
    if (!std::holds_alternative<List>(storage_)) {
        return items;
    }

    const List& list = std::get<List>(storage_);
    items.reserve(list.size());
    for (const Data& item : list) {
        items.push_back(from_data(item));
    }
    return items;
}

std::vector<std::pair<std::string, Value>> Value::object_items() const {
    std::vector<std::pair<std::string, Value>> items;
    if (!std::holds_alternative<Object>(storage_)) {
        return items;
    }

    const Object& object = std::get<Object>(storage_);
    items.reserve(object.size());
    for (const auto& [key, value] : object) {
        items.emplace_back(key, from_data(value));
    }
    return items;
}

Value Value::from_data(const Data& data) {
    return value_from_data(data);
}

}
