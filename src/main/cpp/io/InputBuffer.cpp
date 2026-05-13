#include "io/InputBuffer.h"

#include <cerrno>
#include <fstream>
#include <system_error>

#ifdef __linux__
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace prebyte {

namespace {

constexpr std::size_t mmap_min_size = 16 * 1024;

#ifdef __linux__
std::string read_owned_fd(int fd, std::size_t size) {
    if (::lseek(fd, 0, SEEK_SET) < 0) {
        throw std::system_error(errno, std::generic_category(), "lseek");
    }

    std::string content(size, '\0');
    std::size_t offset = 0;
    while (offset < size) {
        const ssize_t bytes = ::read(fd, content.data() + offset, size - offset);
        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "read");
        }
        if (bytes == 0) {
            break;
        }
        offset += static_cast<std::size_t>(bytes);
    }
    content.resize(offset);
    return content;
}
#endif

std::string read_owned_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::system_error(errno, std::generic_category(), "open");
    }

    stream.seekg(0, std::ios::end);
    const std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    if (size <= 0) {
        return {};
    }

    std::string content(static_cast<std::size_t>(size), '\0');
    stream.read(content.data(), size);
    if (!stream) {
        content.resize(static_cast<std::size_t>(stream.gcount()));
    }
    return content;
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
#endif

}

InputBuffer InputBuffer::from_file(const std::filesystem::path& path) {
#ifdef __linux__
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        throw std::system_error(errno, std::generic_category(), "open");
    }

    ScopedFd scoped_fd(fd);
    struct stat file_stat {};
    if (::fstat(fd, &file_stat) != 0) {
        throw std::system_error(errno, std::generic_category(), "fstat");
    }

    if (!S_ISREG(file_stat.st_mode) || file_stat.st_size <= 0
        || static_cast<std::size_t>(file_stat.st_size) < mmap_min_size) {
        return from_owned(read_owned_fd(fd, file_stat.st_size > 0 ? static_cast<std::size_t>(file_stat.st_size) : 0));
    }

    void* mapping = ::mmap(nullptr, static_cast<std::size_t>(file_stat.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapping == MAP_FAILED) {
        return from_owned(read_owned_fd(fd, static_cast<std::size_t>(file_stat.st_size)));
    }

    InputBuffer buffer;
    buffer.mapped_data_ = static_cast<const char*>(mapping);
    buffer.mapped_size_ = static_cast<std::size_t>(file_stat.st_size);
    return buffer;
#else
    return from_owned(read_owned_file(path));
#endif
}

InputBuffer InputBuffer::from_owned(std::string data) {
    InputBuffer buffer;
    buffer.owned_ = std::move(data);
    return buffer;
}

InputBuffer::InputBuffer(InputBuffer&& other) noexcept {
    move_from(std::move(other));
}

InputBuffer& InputBuffer::operator=(InputBuffer&& other) noexcept {
    if (this != &other) {
        reset();
        move_from(std::move(other));
    }
    return *this;
}

InputBuffer::~InputBuffer() {
    reset();
}

std::string_view InputBuffer::view() const {
    if (mapped_data_ != nullptr) {
        return {mapped_data_, mapped_size_};
    }
    return owned_;
}

bool InputBuffer::empty() const {
    return size() == 0;
}

std::size_t InputBuffer::size() const {
    return mapped_data_ != nullptr ? mapped_size_ : owned_.size();
}

void InputBuffer::reset() noexcept {
#ifdef __linux__
    if (mapped_data_ != nullptr) {
        ::munmap(const_cast<char*>(mapped_data_), mapped_size_);
    }
#endif
    owned_.clear();
    mapped_data_ = nullptr;
    mapped_size_ = 0;
}

void InputBuffer::move_from(InputBuffer&& other) noexcept {
    owned_ = std::move(other.owned_);
    mapped_data_ = other.mapped_data_;
    mapped_size_ = other.mapped_size_;
    other.mapped_data_ = nullptr;
    other.mapped_size_ = 0;
}

}
