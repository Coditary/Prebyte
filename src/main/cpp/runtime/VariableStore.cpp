#include "runtime/VariableStore.h"

#include "support/TextUtil.h"

namespace prebyte {

void VariableStore::set(const std::string& name, std::string value) {
    exact_values_[name] = value;
    lower_values_[text::to_lower(name)] = std::move(value);
}

void VariableStore::set_all(const std::map<std::string, std::string>& variables) {
    for (const auto& [name, value] : variables) {
        set(name, value);
    }
}

std::optional<std::string> VariableStore::get(const std::string& name, bool case_sensitive) const {
    if (case_sensitive) {
        auto it = exact_values_.find(name);
        if (it == exact_values_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    auto it = lower_values_.find(text::to_lower(name));
    if (it == lower_values_.end()) {
        return std::nullopt;
    }
    return it->second;
}

const std::map<std::string, std::string>& VariableStore::values() const {
    return exact_values_;
}

}
