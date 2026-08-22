#include "TestHarness.h"

#include "io/InputBuffer.h"
#include "runtime/compiled/CompiledTemplateWriter.h"

#include <chrono>
#include <filesystem>
#include <thread>

namespace {

std::filesystem::path writer_test_root(const std::string& name) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "prebyte-compiled-template-writer-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

bool wait_for_file(const std::filesystem::path& path, const std::string& expected, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        std::error_code error;
        if (std::filesystem::exists(path, error) && !error) {
            const std::string actual = std::string(prebyte::InputBuffer::from_file(path).view());
            if (actual == expected) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

}

TEST_CASE(CompiledTemplateWriter_enqueue_writes_file_asynchronously) {
    const std::filesystem::path root = writer_test_root("async-write");
    const std::filesystem::path output_path = root / "nested" / "template.pbc";
    const std::string bytes = "PBC1-test-bytes";

    prebyte::CompiledTemplateWriter::instance().enqueue(output_path, bytes);
    REQUIRE(wait_for_file(output_path, bytes, std::chrono::seconds(2)));
}

TEST_CASE(CompiledTemplateWriter_ignores_empty_output_path) {
    prebyte::CompiledTemplateWriter::instance().enqueue({}, "ignored");
}

TEST_CASE(CompiledTemplateWriter_deduplicates_pending_path_enqueue) {
    const std::filesystem::path root = writer_test_root("dedupe");
    const std::filesystem::path output_path = root / "template.pbc";

    prebyte::CompiledTemplateWriter::instance().enqueue(output_path, "first");
    prebyte::CompiledTemplateWriter::instance().enqueue(output_path, "second");

    REQUIRE(wait_for_file(output_path, "first", std::chrono::seconds(2)));
    const std::string actual = std::string(prebyte::InputBuffer::from_file(output_path).view());
    REQUIRE_EQ(actual, std::string("first"));
}

TEST_CASE(CompiledTemplateWriter_creates_parent_directories) {
    const std::filesystem::path root = writer_test_root("mkdirs");
    const std::filesystem::path output_path = root / "deep" / "nested" / "template.pbc";
    const std::string bytes = "compiled";

    prebyte::CompiledTemplateWriter::instance().enqueue(output_path, bytes);
    REQUIRE(wait_for_file(output_path, bytes, std::chrono::seconds(2)));
}
