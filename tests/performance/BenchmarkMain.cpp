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
#include "app/BatchProcessor.h"
#include "app/Command.h"

namespace {

struct BenchmarkRow {
    std::string name;
    long long micros = 0;
    std::size_t output_bytes = 0;
    std::size_t lua_cache_hits = 0;
    std::size_t lua_cache_misses = 0;
    std::optional<long long> per_entry_micros;
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

std::tm local_time_for(std::time_t value) {
    std::tm local_time{};
#ifdef _WIN32
    localtime_s(&local_time, &value);
#else
    localtime_r(&value, &local_time);
#endif
    return local_time;
}

BenchmarkRow run_render_case(const std::string& name, const std::string& input_path,
                             const std::vector<std::string>& define_args) {
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

BenchmarkRow run_batch_case(const std::string& name, const std::filesystem::path& template_path,
                            const std::filesystem::path& batch_path, std::size_t entry_count) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = template_path;
    command.batch_path = batch_path;

    const auto start = std::chrono::steady_clock::now();
    prebyte::BatchProcessor processor;
    const std::string output = processor.execute(command);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    BenchmarkRow row;
    row.name = name;
    row.micros = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    row.output_bytes = output.size();
    row.per_entry_micros = entry_count == 0 ? std::nullopt : std::optional<long long>(row.micros / static_cast<long long>(entry_count));
    return row;
}

void append_section(std::vector<std::string>& lines, const std::string& title, const std::vector<BenchmarkRow>& rows,
                    long long baseline_micros, const std::filesystem::path& history_path) {
    lines.push_back("### " + title);
    lines.push_back("");
    lines.push_back("| Case | Time (us) | Per entry (us) | Delta vs baseline (us) | Delta vs prev (us) | Lua cache hits | Lua cache misses | Output bytes |");
    lines.push_back("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |");
    for (const BenchmarkRow& row : rows) {
        std::ostringstream line;
        line << "| " << row.name
             << " | " << row.micros
             << " | " << (row.per_entry_micros.has_value() ? std::to_string(*row.per_entry_micros) : "-")
             << " | " << format_delta(row.micros, baseline_micros, true)
             << " | " << format_delta(row.micros, previous_time_for_case(history_path, row.name), false)
             << " | " << row.lua_cache_hits
             << " | " << row.lua_cache_misses
             << " | " << row.output_bytes
             << " |";
        lines.push_back(line.str());
    }
    lines.push_back("");
}

}

int main() {
    const std::filesystem::path history_path = "tests/performance/history.md";
    std::filesystem::create_directories(history_path.parent_path());

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    const std::tm local_time = local_time_for(now_time);

    const std::vector<BenchmarkRow> render_rows = {
        run_render_case("simple-variable", "tests/fixtures/render_simple/input.txt", {"name=Ada"}),
        run_render_case("if-include", "tests/fixtures/render_include_if/input.txt", {"name=Ada", "enabled=true"}),
        run_render_case("profile-merge", "tests/fixtures/settings_profile_merge/input.txt", {"name=Ada"}),
        run_render_case("lua-inline", "tests/fixtures/lua_inline/input.txt", {"name=Ada"}),
        run_render_case("lua-repeated", "tests/fixtures/lua_repeated/input.txt", {"name=Ada"}),
        run_render_case("lua-condition", "tests/fixtures/lua_condition/input.txt", {"enabled=true", "name=Ada"}),
    };

    constexpr std::size_t batch_entry_count = 8;
    const std::vector<BenchmarkRow> batch_rows = {
        run_batch_case("batch-variable", "tests/fixtures/batch_simple/template.txt",
                       "tests/fixtures/batch_simple/data.json", batch_entry_count),
    };

    const long long baseline_micros = render_rows.front().micros;

    std::vector<std::string> lines;
    lines.push_back([&]() {
        std::ostringstream stream;
        stream << "## " << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
        return stream.str();
    }());
    lines.push_back("");
    append_section(lines, "Single render (CLI)", render_rows, baseline_micros, history_path);
    append_section(lines, "Batch (CLI, 8 entries)", batch_rows, baseline_micros, history_path);

    const bool write_header = !std::filesystem::exists(history_path) || std::filesystem::file_size(history_path) == 0;
    std::ofstream history(history_path, std::ios::app);
    if (write_header) {
        history << "# Benchmark History\n\n";
        history << "Every benchmark run appends a new timestamped section. Compare sections to track speed over time.\n\n";
        history << "Sections:\n\n";
        history << "1. Single render (CLI): one `AppRunner::render_report()` call per case.\n";
        history << "2. Batch (CLI): one `BatchProcessor::execute()` call per case.\n\n";
    }
    for (const std::string& line : lines) {
        history << line << '\n';
    }

    std::cout << "Wrote benchmark history to " << history_path << '\n';
    return 0;
}
