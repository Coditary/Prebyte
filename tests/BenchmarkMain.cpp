#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <ctime>
#include <vector>

#include "app/AppRunner.h"
#include "app/Command.h"

namespace {

struct BenchmarkRow {
    std::string name;
    long long micros = 0;
    std::size_t output_bytes = 0;
    std::size_t lua_cache_hits = 0;
    std::size_t lua_cache_misses = 0;
};

std::string trim(std::string value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return value.substr(start, end - start);
}

std::optional<long long> previous_time_for_case(const std::filesystem::path& history_path, const std::string& name) {
    if (!std::filesystem::exists(history_path)) {
        return std::nullopt;
    }

    std::ifstream history(history_path);
    std::string line;
    std::optional<long long> previous_time;
    while (std::getline(history, line)) {
        if (line.empty() || line.front() != '|') {
            continue;
        }

        std::vector<std::string> columns;
        std::stringstream line_stream(line);
        std::string column;
        while (std::getline(line_stream, column, '|')) {
            columns.push_back(trim(column));
        }

        if (columns.size() < 3 || columns[1] == "Case" || columns[1] == "---") {
            continue;
        }

        try {
            const long long time = std::stoll(columns[2]);
            if (columns[1] == name) {
                previous_time = time;
            }
        } catch (const std::exception&) {
            continue;
        }
    }

    return previous_time;
}

std::string format_delta(long long current, std::optional<long long> baseline, bool label_baseline) {
    if (!baseline.has_value()) {
        return "new";
    }

    const long long delta = current - *baseline;
    if (delta == 0 && label_baseline) {
        return "baseline";
    }

    std::ostringstream stream;
    stream << std::showpos << delta;
    return stream.str();
}

BenchmarkRow run_case(const std::string& name, const std::string& input_path, const std::vector<std::string>& define_args) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = input_path;
    command.define_args = define_args;

    prebyte::AppRunner runner;
    const prebyte::RenderReport report = runner.render_report(command);

    BenchmarkRow row;
    row.name = name;
    row.micros = report.elapsed_micros;
    row.output_bytes = report.output.size();
    row.lua_cache_hits = report.lua_cache_hits;
    row.lua_cache_misses = report.lua_cache_misses;
    return row;
}

}

int main() {
    const std::filesystem::path history_path = "tests/benchmarks/history.md";
    std::filesystem::create_directories(history_path.parent_path());

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    const std::tm local_time = *std::localtime(&now_time);
    const std::vector<BenchmarkRow> rows = {
        run_case("simple-variable", "tests/fixtures/render_simple/input.txt", {"name=Ada"}),
        run_case("if-include", "tests/fixtures/render_include_if/input.txt", {"name=Ada", "enabled=true"}),
        run_case("profile-merge", "tests/fixtures/settings_profile_merge/input.txt", {"name=Ada"}),
        run_case("lua-inline", "tests/fixtures/lua_inline/input.txt", {"name=Ada"}),
        run_case("lua-repeated", "tests/fixtures/lua_repeated/input.txt", {"name=Ada"}),
        run_case("lua-condition", "tests/fixtures/lua_condition/input.txt", {"enabled=true", "name=Ada"}),
    };
    const long long native_baseline = rows.front().micros;

    std::vector<std::string> lines;
    lines.push_back([&]() {
        std::ostringstream stream;
        stream << "## " << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
        return stream.str();
    }());
    lines.push_back("");
    lines.push_back("| Case | Time (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |");
    lines.push_back("| --- | ---: | ---: | ---: | ---: | ---: | ---: |");
    for (const BenchmarkRow& row : rows) {
        std::ostringstream line;
        line << "| " << row.name
             << " | " << row.micros
             << " | " << format_delta(row.micros, native_baseline, true)
             << " | " << format_delta(row.micros, previous_time_for_case(history_path, row.name), false)
             << " | " << row.lua_cache_hits
             << " | " << row.lua_cache_misses
             << " | " << row.output_bytes
             << " |";
        lines.push_back(line.str());
    }
    lines.push_back("");

    const bool write_header = !std::filesystem::exists(history_path) || std::filesystem::file_size(history_path) == 0;
    std::ofstream history(history_path, std::ios::app);
    if (write_header) {
        history << "# Benchmark History\n\n";
        history << "Every benchmark run appends a new timestamped section. Compare sections to track speed over time.\n\n";
    }
    for (const std::string& line : lines) {
        history << line << '\n';
    }

    std::cout << "Wrote benchmark history to " << history_path << '\n';
    return 0;
}
