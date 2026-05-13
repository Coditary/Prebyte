#pragma once

#include <filesystem>

#include "config/ConfigTypes.h"

namespace prebyte {

class SettingsLoader {
public:
    SettingsData load(const std::filesystem::path& path) const;
};

}
