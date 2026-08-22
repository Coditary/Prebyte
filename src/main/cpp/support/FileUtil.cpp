#include "support/FileUtil.h"

#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace prebyte::file_util {

namespace {

#ifdef _WIN32
using native_fd = int;
constexpr native_fd kInvalidFd = -1;

native_fd open_file(const std::filesystem::path& path, int flags, int mode = 0) {
    return ::_open(path.string().c_str(), flags, mode);
}

ssize_t read_file(native_fd fd, void* buffer, unsigned int count) {
    return ::_read(fd, buffer, count);
}

ssize_t write_file(native_fd fd, const void* buffer, unsigned int count) {
    return ::_write(fd, buffer, count);
}

int close_file(native_fd fd) {
    return ::_close(fd);
}
#else
using native_fd = int;
constexpr native_fd kInvalidFd = -1;

native_fd open_file(const std::filesystem::path& path, int flags, int mode = 0) {
    return ::open(path.c_str(), flags, mode);
}

ssize_t read_file(native_fd fd, void* buffer, std::size_t count) {
    return ::read(fd, buffer, count);
}

ssize_t write_file(native_fd fd, const void* buffer, std::size_t count) {
    return ::write(fd, buffer, count);
}

int close_file(native_fd fd) {
    return ::close(fd);
}
#endif

int open_flags_read() {
#ifdef _WIN32
    return O_RDONLY | O_BINARY;
#else
    return O_RDONLY;
#endif
}

int open_flags_write() {
#ifdef _WIN32
    return O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
#else
    return O_WRONLY | O_CREAT | O_TRUNC;
#endif
}

}

std::string read_text_file(const std::filesystem::path& path) {
    const native_fd fd = open_file(path, open_flags_read());
    if (fd < 0) {
        throw std::runtime_error("Could not open file: " + path.string());
    }

    std::string content;
    char buffer[8192];
    while (true) {
        const ssize_t nbytes = read_file(fd, buffer, sizeof(buffer));
        if (nbytes < 0) {
            close_file(fd);
            throw std::runtime_error("Could not read file: " + path.string());
        }
        if (nbytes == 0) {
            break;
        }
        content.append(buffer, static_cast<std::size_t>(nbytes));
    }

    close_file(fd);
    return content;
}

bool write_text_file(const std::filesystem::path& path, std::string_view content) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);

    const native_fd fd = open_file(path, open_flags_write(), 0644);
    if (fd < 0) {
        return false;
    }

    const char* data = content.data();
    std::size_t remaining = content.size();
    while (remaining > 0) {
        const ssize_t written = write_file(fd, data, static_cast<unsigned int>(remaining));
        if (written <= 0) {
            close_file(fd);
            return false;
        }
        data += written;
        remaining -= static_cast<std::size_t>(written);
    }

    close_file(fd);
    return true;
}

}
