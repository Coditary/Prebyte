#include "CliProcess.h"

#include <array>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace prebyte::test {

namespace {

#ifndef _WIN32

std::string read_pipe_data(int fd) {
    std::string output;
    std::array<char, 4096> buffer{};
    while (true) {
        const ssize_t bytes_read = ::read(fd, buffer.data(), buffer.size());
        if (bytes_read <= 0) {
            break;
        }
        output.append(buffer.data(), static_cast<std::size_t>(bytes_read));
    }
    return output;
}

void apply_extra_env(const std::map<std::string, std::string>& extra_env) {
    for (const auto& [name, value] : extra_env) {
        ::setenv(name.c_str(), value.c_str(), 1);
    }
}

ProcessResult run_cli_posix(const std::filesystem::path& executable, const std::vector<std::string>& args,
                            const std::filesystem::path& working_directory,
                            const std::map<std::string, std::string>& extra_env,
                            const std::string& stdin_text) {
    int stdout_pipe[2]{-1, -1};
    int stderr_pipe[2]{-1, -1};
    int stdin_pipe[2]{-1, -1};
    if (::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0
        || (!stdin_text.empty() && ::pipe(stdin_pipe) != 0)) {
        throw std::runtime_error("failed to create subprocess pipes");
    }

    const pid_t child_pid = ::fork();
    if (child_pid < 0) {
        throw std::runtime_error("failed to fork subprocess");
    }

    if (child_pid == 0) {
        apply_extra_env(extra_env);
        if (!working_directory.empty()) {
            std::error_code error;
            std::filesystem::current_path(working_directory, error);
        }

        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stderr_pipe[1], STDERR_FILENO);
        if (!stdin_text.empty()) {
            ::dup2(stdin_pipe[0], STDIN_FILENO);
        }
        ::close(stdout_pipe[0]);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[0]);
        ::close(stderr_pipe[1]);
        if (!stdin_text.empty()) {
            ::close(stdin_pipe[0]);
            ::close(stdin_pipe[1]);
        }

        std::vector<std::string> argv_storage;
        argv_storage.reserve(args.size() + 1);
        argv_storage.push_back(executable.string());
        for (const std::string& arg : args) {
            argv_storage.push_back(arg);
        }

        std::vector<char*> argv_ptrs;
        argv_ptrs.reserve(argv_storage.size() + 1);
        for (std::string& arg : argv_storage) {
            argv_ptrs.push_back(arg.data());
        }
        argv_ptrs.push_back(nullptr);

        ::execv(executable.c_str(), argv_ptrs.data());
        _exit(127);
    }

    ::close(stdout_pipe[1]);
    ::close(stderr_pipe[1]);
    if (!stdin_text.empty()) {
        ::close(stdin_pipe[0]);
        if (!stdin_text.empty()) {
            const ssize_t bytes_written =
                ::write(stdin_pipe[1], stdin_text.data(), static_cast<ssize_t>(stdin_text.size()));
            if (bytes_written < 0) {
                throw std::runtime_error("failed to write subprocess stdin");
            }
        }
        ::close(stdin_pipe[1]);
    }

    ProcessResult result;
    result.stdout_text = read_pipe_data(stdout_pipe[0]);
    result.stderr_text = read_pipe_data(stderr_pipe[0]);
    ::close(stdout_pipe[0]);
    ::close(stderr_pipe[0]);

    int status = 0;
    if (::waitpid(child_pid, &status, 0) < 0) {
        throw std::runtime_error("failed to wait for subprocess");
    }
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else {
        result.exit_code = -1;
    }
    return result;
}

#else

std::wstring to_wide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int required =
        ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (required <= 0) {
        throw std::runtime_error("failed to convert text to wide string");
    }
    std::wstring wide(static_cast<std::size_t>(required), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), required);
    return wide;
}

std::string quote_windows_argument(const std::string& arg) {
    if (arg.empty()) {
        return "\"\"";
    }
    bool needs_quotes = false;
    for (char ch : arg) {
        if (ch == ' ' || ch == '\t' || ch == '"') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) {
        return arg;
    }

    std::string quoted = "\"";
    std::size_t backslashes = 0;
    for (char ch : arg) {
        if (ch == '\\') {
            ++backslashes;
            continue;
        }
        if (ch == '"') {
            quoted.append(backslashes * 2 + 1, '\\');
            backslashes = 0;
            quoted.push_back('"');
            continue;
        }
        if (backslashes != 0) {
            quoted.append(backslashes, '\\');
            backslashes = 0;
        }
        quoted.push_back(ch);
    }
    if (backslashes != 0) {
        quoted.append(backslashes * 2, '\\');
    }
    quoted.push_back('"');
    return quoted;
}

std::wstring build_windows_command_line(const std::filesystem::path& executable,
                                        const std::vector<std::string>& args) {
    std::string command = quote_windows_argument(executable.string());
    for (const std::string& arg : args) {
        command.push_back(' ');
        command += quote_windows_argument(arg);
    }
    return to_wide(command);
}

std::string normalize_process_output(std::string text) {
    std::string normalized;
    normalized.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '\r' && index + 1 < text.size() && text[index + 1] == '\n') {
            normalized.push_back('\n');
            ++index;
            continue;
        }
        normalized.push_back(text[index]);
    }
    return normalized;
}

std::string read_windows_pipe(HANDLE handle) {
    std::string output;
    std::array<char, 4096> buffer{};
    while (true) {
        DWORD bytes_read = 0;
        if (!::ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr)
            || bytes_read == 0) {
            break;
        }
        output.append(buffer.data(), bytes_read);
    }
    return normalize_process_output(std::move(output));
}

ProcessResult run_cli_windows(const std::filesystem::path& executable, const std::vector<std::string>& args,
                              const std::filesystem::path& working_directory,
                              const std::map<std::string, std::string>& extra_env,
                              const std::string& stdin_text) {
    SECURITY_ATTRIBUTES security_attributes{};
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.bInheritHandle = TRUE;

    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    HANDLE stderr_read = nullptr;
    HANDLE stderr_write = nullptr;
    HANDLE stdin_read = nullptr;
    HANDLE stdin_write = nullptr;
    if (!::CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0)
        || !::CreatePipe(&stderr_read, &stderr_write, &security_attributes, 0)
        || (!stdin_text.empty() && !::CreatePipe(&stdin_read, &stdin_write, &security_attributes, 0))) {
        throw std::runtime_error("failed to create subprocess pipes");
    }

    ::SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);
    if (!stdin_text.empty()) {
        ::SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);
    }

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdOutput = stdout_write;
    startup_info.hStdError = stderr_write;
    startup_info.hStdInput = stdin_text.empty() ? ::GetStdHandle(STD_INPUT_HANDLE) : stdin_read;

    PROCESS_INFORMATION process_info{};
    const std::wstring command_line = build_windows_command_line(executable, args);
    std::vector<wchar_t> mutable_command_line(command_line.begin(), command_line.end());
    mutable_command_line.push_back(L'\0');

    std::wstring environment_block;
    if (!extra_env.empty()) {
        for (const auto& [name, value] : extra_env) {
            environment_block += to_wide(name + '=' + value);
            environment_block.push_back(L'\0');
        }
        environment_block.push_back(L'\0');
    }

    const BOOL created = ::CreateProcessW(
        to_wide(executable.string()).c_str(),
        mutable_command_line.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        environment_block.empty() ? nullptr : environment_block.data(),
        working_directory.empty() ? nullptr : to_wide(working_directory.string()).c_str(),
        &startup_info,
        &process_info);
    ::CloseHandle(stdout_write);
    ::CloseHandle(stderr_write);
    if (!stdin_text.empty()) {
        ::CloseHandle(stdin_read);
    }

    if (!created) {
        ::CloseHandle(stdout_read);
        ::CloseHandle(stderr_read);
        throw std::runtime_error("failed to create subprocess");
    }

    ProcessResult result;
    if (!stdin_text.empty()) {
        DWORD bytes_written = 0;
        if (!::WriteFile(stdin_write, stdin_text.data(), static_cast<DWORD>(stdin_text.size()), &bytes_written, nullptr)) {
            ::CloseHandle(stdout_read);
            ::CloseHandle(stderr_read);
            ::CloseHandle(stdin_write);
            ::CloseHandle(process_info.hThread);
            ::CloseHandle(process_info.hProcess);
            throw std::runtime_error("failed to write subprocess stdin");
        }
        ::CloseHandle(stdin_write);
    }

    result.stdout_text = read_windows_pipe(stdout_read);
    result.stderr_text = read_windows_pipe(stderr_read);
    ::CloseHandle(stdout_read);
    ::CloseHandle(stderr_read);

    ::WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 1;
    ::GetExitCodeProcess(process_info.hProcess, &exit_code);
    result.exit_code = static_cast<int>(exit_code);

    ::CloseHandle(process_info.hThread);
    ::CloseHandle(process_info.hProcess);
    return result;
}

#endif

}

std::filesystem::path cli_binary_path() {
    if (const char* override_path = std::getenv("PREBYTE_CLI_BINARY")) {
        return override_path;
    }
#ifdef PREBYTE_CLI_BINARY
    return PREBYTE_CLI_BINARY;
#else
    return "prebyte";
#endif
}

std::filesystem::path cli_working_directory() {
    if (const char* override_path = std::getenv("PREBYTE_CLI_WORKDIR")) {
        return override_path;
    }
#ifdef PREBYTE_CLI_WORKDIR
    return PREBYTE_CLI_WORKDIR;
#else
    return std::filesystem::current_path();
#endif
}

ProcessResult run_cli(const std::vector<std::string>& args, const std::filesystem::path& working_directory,
                      const std::map<std::string, std::string>& extra_env, const std::string& stdin_text) {
    const std::filesystem::path executable = cli_binary_path();
#ifndef _WIN32
    return run_cli_posix(executable, args, working_directory, extra_env, stdin_text);
#else
    return run_cli_windows(executable, args, working_directory, extra_env, stdin_text);
#endif
}

}
