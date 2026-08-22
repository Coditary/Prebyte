#include "TestHarness.h"

#include "CliProcess.h"
#include "support/Version.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace {

#ifndef _WIN32
std::string shell_quote(const std::string& value) {
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('\'');
    return quoted;
}
#else
std::string windows_quote(const std::string& value) {
    std::string quoted = "\"";
    for (char ch : value) {
        if (ch == '"') {
            quoted += "\\\"";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('"');
    return quoted;
}
#endif

int run_packaging_smoke(const std::vector<std::string>& checks) {
    const std::filesystem::path repo_root = prebyte::test::cli_working_directory();
    const std::filesystem::path binary = prebyte::test::cli_binary_path();
    const std::filesystem::path script_path = repo_root / "scripts" / "ci" / "smoke_packaging.py";

    std::ostringstream command;
#ifdef _WIN32
    command << "python " << windows_quote(script_path.string()) << " --binary " << windows_quote(binary.string());
    for (const std::string& check : checks) {
        command << " --checks " << windows_quote(check);
    }
#else
    command << "python3 " << shell_quote(script_path.string()) << " --binary " << shell_quote(binary.string());
    for (const std::string& check : checks) {
        command << " --checks " << shell_quote(check);
    }
#endif

    const int status = std::system(command.str().c_str());
    if (status == -1) {
        throw std::runtime_error("failed to execute packaging smoke script");
    }
#ifdef _WIN32
    return status;
#else
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
#endif
    throw std::runtime_error("packaging smoke script terminated abnormally");
}

void require_packaging_smoke(const std::vector<std::string>& checks) {
    const int exit_code = run_packaging_smoke(checks);
    if (exit_code != 0) {
        throw prebyte::test::AssertionFailure("packaging smoke script failed with exit code "
                                              + std::to_string(exit_code));
    }
}

}

TEST_CASE(PackagingSmoke_binary_release_archive_runs) {
    require_packaging_smoke({"binary"});
}

#ifndef _WIN32
TEST_CASE(PackagingSmoke_reqpack_archive_runs) {
    require_packaging_smoke({"reqpack"});
}

TEST_CASE(PackagingSmoke_reqpack_index_lists_package) {
    require_packaging_smoke({"reqpack", "index"});
}
#endif

TEST_CASE(PackagingSmoke_docker_image_runs_cli) {
    if (std::getenv("PREBYTE_SMOKE_DOCKER") == nullptr) {
        return;
    }
    require_packaging_smoke({"docker"});
}
