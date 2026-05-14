#include "runtime/VariableStore.h"

#include "support/TextUtil.h"

namespace prebyte {

void VariableStore::set(const std::string& name, std::string value) {
    set_value(name, Value(std::move(value)));
}

void VariableStore::set_value(const std::string& name, Value value) {
    const std::string lowered = text::to_lower(name);
    exact_values_[name] = value;
    lower_values_[lowered] = std::move(value);
}

const Value* VariableStore::get_value(std::string_view name, bool case_sensitive) const {
    if (case_sensitive) {
        auto it = exact_values_.find(name);
        if (it == exact_values_.end()) {
            return nullptr;
        }
        return &it->second;
    }

    auto it = lower_values_.find(text::to_lower(std::string(name)));
    if (it == lower_values_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::optional<std::string_view> VariableStore::get_view(std::string_view name, bool case_sensitive) const {
    const Value* value = get_value(name, case_sensitive);
    if (value == nullptr) {
        return std::nullopt;
    }
    return value->try_as_string_view();
}

std::optional<std::string> VariableStore::get(const std::string& name, bool case_sensitive) const {
    if (const Value* value = get_value(name, case_sensitive)) {
        return value->to_string();
    }
    return std::nullopt;
}

const VariableStore::Map& VariableStore::values() const {
    return exact_values_;
}

}
