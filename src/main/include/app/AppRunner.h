#pragma once

#include <cstddef>
#include <string>

#include "app/Command.h"

namespace prebyte {

struct RenderReport {
    std::string output;
    long long elapsed_micros = 0;
    std::size_t lua_cache_hits = 0;
    std::size_t lua_cache_misses = 0;
};

class AppRunner {
public:
    std::string execute(const Command& command) const;
    RenderReport render_report(const Command& command) const;
    void run(const Command& command) const;

private:
    std::string list_rules(const Command& command) const;
    std::string list_vars(const Command& command) const;
    std::string list_profiles(const Command& command) const;
    std::string list_ignores(const Command& command) const;
    std::string explain(const Command& command) const;
    std::string help() const;
    std::string version() const;
};

}
