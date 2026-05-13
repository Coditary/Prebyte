#pragma once

#include <string>
#include <vector>

#include "config/ConfigTypes.h"

namespace prebyte {

class ProfileMerger {
public:
    SettingsData merge(const SettingsData& settings, const std::vector<std::string>& profile_names) const;
};

}
