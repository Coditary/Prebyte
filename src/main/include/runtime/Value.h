#pragma once

#include <string>
#include <variant>

namespace prebyte {

class Value {
public:
    using Storage = std::variant<std::monostate, bool, double, std::string>;

    Value() = default;
    Value(bool value);
    Value(double value);
    Value(std::string value);

    bool is_null() const;
    bool to_bool() const;
    std::string to_string() const;

private:
    Storage storage_;
};

}
