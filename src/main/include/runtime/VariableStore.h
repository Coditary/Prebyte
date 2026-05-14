#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "runtime/Value.h"

namespace prebyte {

class VariableStore {
public:
    using Map = std::map<std::string, Value, std::less<>>;

    void set(const std::string& name, std::string value);
    void set_value(const std::string& name, Value value);
    template <typename MapType>
    void set_all(const MapType& variables) {
        for (const auto& [name, value] : variables) {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>, Value>) {
                set_value(name, value);
            } else {
                set(name, std::string(value));
            }
        }
    }
    std::optional<std::string_view> get_view(std::string_view name, bool case_sensitive) const;
    std::optional<std::string> get(const std::string& name, bool case_sensitive) const;
    const Value* get_value(std::string_view name, bool case_sensitive) const;
    const Map& values() const;

private:
    Map exact_values_;
    Map lower_values_;
};

}
