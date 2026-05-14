#include "TestHarness.h"

#include "io/InputBuffer.h"
#include "io/InputReader.h"
#include "io/OutputWriter.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#ifdef __linux__
#include <unistd.h>
#endif

namespace {

void write_io_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path io_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-io-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

#ifdef __linux__
class ScopedFdRedirect {
public:
    ScopedFdRedirect(int target_fd, int replacement_fd)
        : target_fd_(target_fd), saved_fd_(::dup(target_fd)) {
        if (saved_fd_ < 0 || ::dup2(replacement_fd, target_fd_) < 0) {
            throw std::runtime_error("fd redirect failed");
        }
    }

    ScopedFdRedirect(const ScopedFdRedirect&) = delete;
    ScopedFdRedirect& operator=(const ScopedFdRedirect&) = delete;

    ~ScopedFdRedirect() {
        if (saved_fd_ >= 0) {
            ::dup2(saved_fd_, target_fd_);
            ::close(saved_fd_);
        }
    }

private:
    int target_fd_ = -1;
    int saved_fd_ = -1;
};

std::string read_all_fd(int fd) {
    std::string output;
    char buffer[256];
    while (true) {
        const ssize_t count = ::read(fd, buffer, sizeof(buffer));
        if (count <= 0) {
            break;
        }
        output.append(buffer, static_cast<std::size_t>(count));
    }
    return output;
}
#endif

}

TEST_CASE(InputBuffer_from_owned_move_and_size_paths) {
    prebyte::InputBuffer buffer = prebyte::InputBuffer::from_owned("Ada");
    REQUIRE(!buffer.empty());
    REQUIRE_EQ(buffer.size(), static_cast<std::size_t>(3));
    REQUIRE_EQ(buffer.view(), std::string_view("Ada"));

    prebyte::InputBuffer moved(std::move(buffer));
    REQUIRE_EQ(moved.view(), std::string_view("Ada"));

    prebyte::InputBuffer assigned;
    assigned = std::move(moved);
    REQUIRE_EQ(assigned.view(), std::string_view("Ada"));
}

TEST_CASE(InputBuffer_from_file_covers_small_large_and_empty_files) {
    const std::filesystem::path root = io_test_root("input-buffer-files");
    const std::filesystem::path small = root / "small.txt";
    const std::filesystem::path large = root / "large.txt";
    const std::filesystem::path empty = root / "empty.txt";
    write_io_file(small, "Ada");
    write_io_file(large, std::string(20000, 'x'));
    write_io_file(empty, std::string());

    const prebyte::InputBuffer small_buffer = prebyte::InputBuffer::from_file(small);
    REQUIRE_EQ(small_buffer.view(), std::string_view("Ada"));

    const prebyte::InputBuffer large_buffer = prebyte::InputBuffer::from_file(large);
    REQUIRE_EQ(large_buffer.size(), static_cast<std::size_t>(20000));
    REQUIRE_EQ(large_buffer.view().front(), 'x');
    REQUIRE_EQ(large_buffer.view().back(), 'x');

    const prebyte::InputBuffer empty_buffer = prebyte::InputBuffer::from_file(empty);
    REQUIRE(empty_buffer.empty());
}

TEST_CASE(InputBuffer_from_file_missing_path_throws) {
    REQUIRE_THROWS_AS(prebyte::InputBuffer::from_file("/definitely/missing/prebyte-input-buffer.txt"), std::system_error);
}

TEST_CASE(InputReader_reads_file_and_wraps_missing_file_errors) {
    const std::filesystem::path root = io_test_root("input-reader-file");
    const std::filesystem::path file = root / "input.txt";
    write_io_file(file, "Grace");

    prebyte::InputReader reader;
    const prebyte::InputBuffer buffer = reader.read(file);
    REQUIRE_EQ(buffer.view(), std::string_view("Grace"));

    REQUIRE_THROWS_AS(reader.read(root / "missing.txt"), prebyte::DiagnosticError);
}

#ifdef __linux__
TEST_CASE(InputReader_reads_stdin_when_no_path_is_given) {
    int pipe_fds[2] = {-1, -1};
    REQUIRE(::pipe(pipe_fds) == 0);
    REQUIRE(::write(pipe_fds[1], "stdin-data", 10) == 10);
    ::close(pipe_fds[1]);

    prebyte::InputReader reader;
    std::cin.clear();
    {
        ScopedFdRedirect redirect(STDIN_FILENO, pipe_fds[0]);
        ::close(pipe_fds[0]);
        const prebyte::InputBuffer buffer = reader.read(std::nullopt);
        REQUIRE_EQ(buffer.view(), std::string_view("stdin-data"));
    }
    std::cin.clear();
}

TEST_CASE(OutputWriter_writes_stdout_when_no_path_is_given) {
    int pipe_fds[2] = {-1, -1};
    REQUIRE(::pipe(pipe_fds) == 0);

    prebyte::OutputWriter writer;
    {
        ScopedFdRedirect redirect(STDOUT_FILENO, pipe_fds[1]);
        ::close(pipe_fds[1]);
        writer.write("stdout-data", std::nullopt);
    }

    const std::string output = read_all_fd(pipe_fds[0]);
    ::close(pipe_fds[0]);
    REQUIRE_EQ(output, std::string("stdout-data"));
}
#endif

TEST_CASE(OutputWriter_writes_files_and_reports_open_errors) {
    const std::filesystem::path root = io_test_root("output-writer");
    const std::filesystem::path output = root / "out.txt";
    const std::filesystem::path missing = root / "missing" / "out.txt";

    prebyte::OutputWriter writer;
    writer.write("Hello", output);
    REQUIRE_EQ(prebyte::InputBuffer::from_file(output).view(), std::string_view("Hello"));

    REQUIRE_THROWS_AS(writer.write("Hello", missing), prebyte::DiagnosticError);
}

TEST_CASE(OutputWriter_utf16_file_output_writes_bom_and_little_endian_units) {
    const std::filesystem::path root = io_test_root("output-writer-utf16");
    const std::filesystem::path output = root / "out.txt";

    prebyte::OutputWriter writer;
    writer.write("A\xC3\xA4", output, "utf-16");

    const std::string bytes = std::string(prebyte::InputBuffer::from_file(output).view());
    REQUIRE_EQ(bytes.size(), static_cast<std::size_t>(6));
    REQUIRE_EQ(static_cast<unsigned char>(bytes[0]), 0xFFu);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[1]), 0xFEu);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[2]), 0x41u);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[3]), 0x00u);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[4]), 0xE4u);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[5]), 0x00u);
}

TEST_CASE(OutputWriter_rejects_invalid_output_encoding) {
    const std::filesystem::path root = io_test_root("output-writer-invalid-encoding");
    const std::filesystem::path output = root / "out.txt";

    prebyte::OutputWriter writer;
    REQUIRE_THROWS_AS(writer.write("Hello", output, "latin-1"), prebyte::DiagnosticError);
}
