#pragma once

#include <string>
#include <vector>

#include "app/Command.h"

namespace prebyte {

class CommandParser {
public:
    Command parse(const std::vector<std::string>& args) const;
};

}
