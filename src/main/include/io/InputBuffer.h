#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace prebyte {

class InputBuffer {
public:
    InputBuffer() = default;
    InputBuffer(const InputBuffer&) = delete;
    InputBuffer& operator=(const InputBuffer&) = delete;
    InputBuffer(InputBuffer&& other) noexcept;
    InputBuffer& operator=(InputBuffer&& other) noexcept;
    ~InputBuffer();

    static InputBuffer from_file(const std::filesystem::path& path);
    static InputBuffer from_owned(std::string data);

    std::string_view view() const;
    bool empty() const;
    std::size_t size() const;

private:
    void reset() noexcept;
    void move_from(InputBuffer&& other) noexcept;

    std::string owned_;
    const char* mapped_data_ = nullptr;
    std::size_t mapped_size_ = 0;
};

}
