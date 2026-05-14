#pragma once

#include "datatypes/Data.h"

#include <cstddef>
#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace prebyte {

class Value {
public:
    struct BorrowedData {
        const Data* data = nullptr;
    };

    using Object = Data::Map;
    using List = Data::Array;
    using Storage = std::variant<std::monostate, bool, double, std::string, std::string_view, Object, List, BorrowedData>;

    Value() = default;
    Value(bool value);
    Value(double value);
    Value(std::string value);
    static Value borrowed(std::string_view value);
    static Value borrowed_data(const Data& data);
    static Value object(Object value);
    static Value list(List value);

    bool is_null() const;
    bool is_object() const;
    bool is_list() const;
    std::optional<bool> try_as_bool() const;
    std::optional<double> try_as_number() const;
    bool to_bool() const;
    std::size_t length() const;
    std::optional<std::string_view> try_as_string_view() const;
    std::string_view as_string_view() const;
    const Object* try_as_object() const;
    const List* try_as_list() const;
    std::string to_string() const;
    void append_to(std::string& output) const;
    bool equals(const Value& other) const;
    std::optional<std::strong_ordering> compare_scalar(const Value& other) const;
    std::optional<Value> member(std::string_view key) const;
    std::optional<Value> index(std::size_t index) const;
    std::vector<Value> list_items() const;
    std::vector<std::pair<std::string, Value>> object_items() const;

private:
    explicit Value(Storage storage);

    Storage storage_;

public:
    static Value from_data(const Data& data);
};

}
