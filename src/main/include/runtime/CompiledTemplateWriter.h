#pragma once

#include <filesystem>
#include <string>

namespace prebyte {

class CompiledTemplateWriter {
public:
    static CompiledTemplateWriter& instance();

    void enqueue(std::filesystem::path output_path, std::string bytes);

private:
    CompiledTemplateWriter();
    ~CompiledTemplateWriter();
    CompiledTemplateWriter(const CompiledTemplateWriter&) = delete;
    CompiledTemplateWriter& operator=(const CompiledTemplateWriter&) = delete;

    void run();

    struct Impl;
    Impl* impl_ = nullptr;
};

}
