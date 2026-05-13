#pragma once

#include <map>
#include <optional>
#include <string>

namespace prebyte {

class VariableStore {
public:
    void set(const std::string& name, std::string value);
    void set_all(const std::map<std::string, std::string>& variables);
    std::optional<std::string> get(const std::string& name, bool case_sensitive) const;
    const std::map<std::string, std::string>& values() const;

private:
    std::map<std::string, std::string> exact_values_;
    std::map<std::string, std::string> lower_values_;
};

}
