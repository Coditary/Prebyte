#include "io/OutputWriter.h"

#include "support/Diagnostic.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#endif

namespace prebyte {

namespace {

Diagnostic make_write_error(const std::string& message, const std::filesystem::path& path = {}) {
    Diagnostic diagnostic;
    diagnostic.code = "IO002";
    diagnostic.message = message;
    diagnostic.span.file_path = path.string();
    return diagnostic;
}

#ifdef __linux__
class ScopedFd {
public:
    explicit ScopedFd(int fd) : fd_(fd) {}
    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;
    ~ScopedFd() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    int get() const {
        return fd_;
    }

private:
    int fd_ = -1;
};

void write_all(int fd, std::string_view output, const std::filesystem::path& path) {
    std::size_t offset = 0;
    while (offset < output.size()) {
        const ssize_t written = ::write(fd, output.data() + offset, output.size() - offset);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw DiagnosticError(make_write_error("Cannot write output file: " + path.string(), path));
        }
        offset += static_cast<std::size_t>(written);
    }
}
#endif

}

void OutputWriter::write(std::string_view output, const std::optional<std::filesystem::path>& output_path) const {
#ifdef __linux__
    if (!output_path.has_value()) {
        std::size_t offset = 0;
        while (offset < output.size()) {
            const ssize_t written = ::write(STDOUT_FILENO, output.data() + offset, output.size() - offset);
            if (written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw DiagnosticError(make_write_error("Cannot write output to stdout"));
            }
            offset += static_cast<std::size_t>(written);
        }
        return;
    }

    const int fd = ::open(output_path->c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    if (fd < 0) {
        throw DiagnosticError(make_write_error("Cannot write output file: " + output_path->string(), *output_path));
    }

    ScopedFd scoped_fd(fd);
    write_all(fd, output, *output_path);
#else
    if (!output_path.has_value()) {
        std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
        return;
    }

    std::ofstream file(*output_path, std::ios::binary);
    if (!file) {
        throw DiagnosticError(make_write_error("Cannot write output file: " + output_path->string(), *output_path));
    }

    file.write(output.data(), static_cast<std::streamsize>(output.size()));
#endif
}

}
