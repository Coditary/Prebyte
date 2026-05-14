#include "runtime/BuiltinRegistry.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <format>
#include <random>
#include <sstream>

#if defined(__unix__) || defined(__APPLE__)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
#include <array>
#endif

namespace prebyte {

namespace {

std::string format_two_digits(int value) {
    return std::format("{:02}", value);
}

std::string format_iso_date(const std::tm& local_time) {
    return std::format("{:04}-{:02}-{:02}", local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday);
}

std::string format_iso_timestamp(const std::tm& local_time) {
    return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}",
                       local_time.tm_year + 1900,
                       local_time.tm_mon + 1,
                       local_time.tm_mday,
                       local_time.tm_hour,
                       local_time.tm_min,
                       local_time.tm_sec);
}

std::string lookup_user() {
    if (const char* env_user = std::getenv("USER"); env_user != nullptr && *env_user != '\0') {
        return env_user;
    }
    if (const char* env_user = std::getenv("USERNAME"); env_user != nullptr && *env_user != '\0') {
        return env_user;
    }
#if defined(__unix__) || defined(__APPLE__)
    if (const passwd* entry = getpwuid(getuid()); entry != nullptr && entry->pw_name != nullptr) {
        return entry->pw_name;
    }
#endif
    return {};
}

std::string lookup_host() {
    if (const char* env_host = std::getenv("HOSTNAME"); env_host != nullptr && *env_host != '\0') {
        return env_host;
    }
    if (const char* env_host = std::getenv("COMPUTERNAME"); env_host != nullptr && *env_host != '\0') {
        return env_host;
    }
#if defined(__unix__) || defined(__APPLE__)
    std::array<char, 256> buffer{};
    if (gethostname(buffer.data(), buffer.size()) == 0) {
        buffer.back() = '\0';
        return buffer.data();
    }
#endif
    return {};
}

std::string detect_os() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::string lookup_working_dir() {
    std::error_code error;
    const std::filesystem::path cwd = std::filesystem::current_path(error);
    if (error) {
        return {};
    }
    return cwd.lexically_normal().string();
}

std::string make_uuid(std::mt19937_64& rng) {
    std::uniform_int_distribution<std::uint32_t> dist(0, 0xffffffffu);
    std::uint32_t part1 = dist(rng);
    std::uint16_t part2 = static_cast<std::uint16_t>(dist(rng) & 0xffffu);
    std::uint16_t part3 = static_cast<std::uint16_t>((dist(rng) & 0x0fffu) | 0x4000u);
    std::uint16_t part4 = static_cast<std::uint16_t>((dist(rng) & 0x3fffu) | 0x8000u);
    std::uint64_t part5 = (static_cast<std::uint64_t>(dist(rng)) << 16) | (dist(rng) & 0xffffu);
    return std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:012x}", part1, part2, part3, part4, part5 & 0xffffffffffffULL);
}

RenderSession::BuiltinSnapshot make_snapshot() {
    RenderSession::BuiltinSnapshot snapshot;
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    const std::tm local_time = *std::localtime(&now_time);

    snapshot.time = std::format("{:02}:{:02}:{:02}", local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
    snapshot.date = format_iso_date(local_time);
    snapshot.timestamp = format_iso_timestamp(local_time);
    snapshot.year = std::format("{:04}", local_time.tm_year + 1900);
    snapshot.month = format_two_digits(local_time.tm_mon + 1);
    snapshot.day = format_two_digits(local_time.tm_mday);
    snapshot.unix_epoch = std::to_string(static_cast<long long>(now_time));
    snapshot.user = lookup_user();
    snapshot.host = lookup_host();
    snapshot.os = detect_os();
    snapshot.working_dir = lookup_working_dir();

    std::random_device device;
    std::mt19937_64 rng(device());
    const std::uint32_t random_value = static_cast<std::uint32_t>(rng() & 0xffffffffu);
    snapshot.random = std::to_string(random_value);
    snapshot.uuid = make_uuid(rng);
    return snapshot;
}

const RenderSession::BuiltinSnapshot& snapshot_for(const RenderSession& session) {
    if (!session.builtin_snapshot.has_value()) {
        const_cast<RenderSession&>(session).builtin_snapshot = make_snapshot();
    }
    return *session.builtin_snapshot;
}

std::string filename_without_extension(const std::filesystem::path& path) {
    return path.stem().string();
}

std::string extension_without_dot(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(extension.begin());
    }
    return extension;
}

std::string directory_for(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }
    std::error_code error;
    const std::filesystem::path absolute = path.is_absolute() ? path.lexically_normal() : std::filesystem::absolute(path, error).lexically_normal();
    if (error) {
        return path.parent_path().lexically_normal().string();
    }
    return absolute.parent_path().string();
}

}

std::optional<std::string> BuiltinRegistry::resolve(const std::string& name, const SourceSpan& span,
                                                    const std::filesystem::path& current_file,
                                                    const RenderSession& session) const {
    const auto& snapshot = snapshot_for(session);
    if (name == "__TIME__") {
        return snapshot.time;
    }
    if (name == "__LINE__") {
        return std::to_string(span.start.line);
    }
    if (name == "__FILE__") {
        return current_file.string();
    }
    if (name == "__FILENAME__") {
        return filename_without_extension(current_file);
    }
    if (name == "__DIR__") {
        return directory_for(current_file);
    }
    if (name == "__EXTENSION__") {
        return extension_without_dot(current_file);
    }
    if (name == "__DATE__") {
        return snapshot.date;
    }
    if (name == "__TIMESTAMP__") {
        return snapshot.timestamp;
    }
    if (name == "__YEAR__") {
        return snapshot.year;
    }
    if (name == "__MONTH__") {
        return snapshot.month;
    }
    if (name == "__DAY__") {
        return snapshot.day;
    }
    if (name == "__UNIX_EPOCH__") {
        return snapshot.unix_epoch;
    }
    if (name == "__USER__") {
        return snapshot.user;
    }
    if (name == "__HOST__") {
        return snapshot.host;
    }
    if (name == "__OS__") {
        return snapshot.os;
    }
    if (name == "__WORKING_DIR__") {
        return snapshot.working_dir;
    }
    if (name == "__UUID__") {
        return snapshot.uuid;
    }
    if (name == "__RANDOM__") {
        return snapshot.random;
    }
    return std::nullopt;
}

}
