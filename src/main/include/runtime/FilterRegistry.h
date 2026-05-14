#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "runtime/Value.h"

namespace prebyte {

class FilterRegistry {
public:
    Value apply(std::string_view name, const std::vector<Value>& arguments) const;
};

}
