#include "runtime/BuiltinRegistry.h"

#include <chrono>
#include <ctime>
#include <format>

namespace prebyte {

std::optional<std::string> BuiltinRegistry::resolve(const std::string& name, const SourceSpan& span,
                                                    const std::filesystem::path& current_file) const {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    const std::tm local_time = *std::localtime(&now_time);

    if (name == "__TIME__") {
        return std::format("{:02}:{:02}:{:02}", local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
    }
    if (name == "__LINE__") {
        return std::to_string(span.start.line);
    }
    if (name == "__FILE__") {
        return current_file.string();
    }
    return std::nullopt;
}

}
