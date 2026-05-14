#include "io/OutputWriter.h"

#include "support/TextUtil.h"
#include "support/Diagnostic.h"

#include <cstdint>
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

std::string normalize_encoding_name(std::string_view encoding) {
    return text::to_lower(text::trim(std::string(encoding)));
}

void append_utf16_unit(std::string& output, std::uint16_t unit) {
    output.push_back(static_cast<char>(unit & 0xFFu));
    output.push_back(static_cast<char>((unit >> 8) & 0xFFu));
}

std::uint32_t decode_utf8_code_point(std::string_view input, std::size_t& offset,
                                     const std::filesystem::path& path) {
    const unsigned char first = static_cast<unsigned char>(input[offset]);
    if (first < 0x80) {
        ++offset;
        return first;
    }

    std::size_t length = 0;
    std::uint32_t code_point = 0;
    if ((first & 0xE0u) == 0xC0u) {
        length = 2;
        code_point = first & 0x1Fu;
    } else if ((first & 0xF0u) == 0xE0u) {
        length = 3;
        code_point = first & 0x0Fu;
    } else if ((first & 0xF8u) == 0xF0u) {
        length = 4;
        code_point = first & 0x07u;
    } else {
        throw DiagnosticError(make_write_error("Cannot encode output as utf-16: invalid UTF-8 sequence", path));
    }

    if (offset + length > input.size()) {
        throw DiagnosticError(make_write_error("Cannot encode output as utf-16: truncated UTF-8 sequence", path));
    }

    for (std::size_t index = 1; index < length; ++index) {
        const unsigned char next = static_cast<unsigned char>(input[offset + index]);
        if ((next & 0xC0u) != 0x80u) {
            throw DiagnosticError(make_write_error("Cannot encode output as utf-16: invalid UTF-8 sequence", path));
        }
        code_point = (code_point << 6u) | (next & 0x3Fu);
    }

    const bool overlong = (length == 2 && code_point < 0x80u)
        || (length == 3 && code_point < 0x800u)
        || (length == 4 && code_point < 0x10000u);
    if (overlong || code_point > 0x10FFFFu || (code_point >= 0xD800u && code_point <= 0xDFFFu)) {
        throw DiagnosticError(make_write_error("Cannot encode output as utf-16: invalid UTF-8 sequence", path));
    }

    offset += length;
    return code_point;
}

std::string encode_utf16(std::string_view output, const std::filesystem::path& path) {
    std::string encoded;
    encoded.reserve(2 + output.size() * 2);
    encoded.push_back(static_cast<char>(0xFF));
    encoded.push_back(static_cast<char>(0xFE));

    std::size_t offset = 0;
    while (offset < output.size()) {
        const std::uint32_t code_point = decode_utf8_code_point(output, offset, path);
        if (code_point <= 0xFFFFu) {
            append_utf16_unit(encoded, static_cast<std::uint16_t>(code_point));
            continue;
        }

        const std::uint32_t surrogate = code_point - 0x10000u;
        const std::uint16_t high = static_cast<std::uint16_t>(0xD800u + (surrogate >> 10u));
        const std::uint16_t low = static_cast<std::uint16_t>(0xDC00u + (surrogate & 0x3FFu));
        append_utf16_unit(encoded, high);
        append_utf16_unit(encoded, low);
    }

    return encoded;
}

std::string encode_output(std::string_view output, std::string_view encoding,
                          const std::filesystem::path& path) {
    const std::string normalized = normalize_encoding_name(encoding);
    if (normalized.empty() || normalized == "utf-8") {
        return std::string(output);
    }
    if (normalized == "utf-16") {
        return encode_utf16(output, path);
    }
    throw DiagnosticError(make_write_error("Unsupported output encoding: " + normalized, path));
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

void OutputWriter::write(std::string_view output, const std::optional<std::filesystem::path>& output_path,
                         std::string_view encoding) const {
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
    const std::string encoded = encode_output(output, encoding, *output_path);
    write_all(fd, encoded, *output_path);
#else
    if (!output_path.has_value()) {
        std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
        return;
    }

    std::ofstream file(*output_path, std::ios::binary);
    if (!file) {
        throw DiagnosticError(make_write_error("Cannot write output file: " + output_path->string(), *output_path));
    }

    const std::string encoded = encode_output(output, encoding, *output_path);
    file.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
#endif
}

}
