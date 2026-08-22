#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace prebyte::test {

struct ProcessResult {
    int exit_code = -1;
    std::string stdout_text;
    std::string stderr_text;
};

std::filesystem::path cli_binary_path();
std::filesystem::path cli_working_directory();

ProcessResult run_cli(const std::vector<std::string>& args,
                      const std::filesystem::path& working_directory = cli_working_directory(),
                      const std::map<std::string, std::string>& extra_env = {},
                      const std::string& stdin_text = {});

}
