#pragma once

#include "app/Command.h"

namespace prebyte {

class BatchProcessor {
public:
    std::string execute(const Command& command) const;
    void run(const Command& command) const;
};

}
