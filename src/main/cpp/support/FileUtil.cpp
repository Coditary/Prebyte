#include "support/FileUtil.h"

#include <fcntl.h>
#include <unistd.h>

#include <stdexcept>
#include <system_error>

namespace prebyte::file_util {

namespace {

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
    const int fd = ::open(path.c_str(), open_flags_read());
    if (fd < 0) {
        throw std::runtime_error("Could not open file: " + path.string());
    }

    std::string content;
    char buffer[8192];
    while (true) {
        const ssize_t nbytes = ::read(fd, buffer, sizeof(buffer));
        if (nbytes < 0) {
            ::close(fd);
            throw std::runtime_error("Could not read file: " + path.string());
        }
        if (nbytes == 0) {
            break;
        }
        content.append(buffer, static_cast<std::size_t>(nbytes));
    }

    ::close(fd);
    return content;
}

bool write_text_file(const std::filesystem::path& path, std::string_view content) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);

    const int fd = ::open(path.c_str(), open_flags_write(), 0644);
    if (fd < 0) {
        return false;
    }

    const char* data = content.data();
    std::size_t remaining = content.size();
    while (remaining > 0) {
        const ssize_t written = ::write(fd, data, remaining);
        if (written <= 0) {
            ::close(fd);
            return false;
        }
        data += written;
        remaining -= static_cast<std::size_t>(written);
    }

    ::close(fd);
    return true;
}

}
