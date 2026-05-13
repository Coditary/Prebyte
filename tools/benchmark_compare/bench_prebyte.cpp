#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "PrebyteEngine.h"

namespace {

using Clock = std::chrono::steady_clock;

struct BenchCase {
    std::string name;
    std::size_t iterations = 0;
    std::function<std::string()> render;
    std::string expected;
};

double benchmark_case(const BenchCase& bench_case) {
    std::vector<double> samples;
    samples.reserve(5);
    for (int run = 0; run < 5; ++run) {
        std::string output;
        const auto start = Clock::now();
        for (std::size_t iteration = 0; iteration < bench_case.iterations; ++iteration) {
            output = bench_case.render();
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
        if (output != bench_case.expected) {
            throw std::runtime_error("Unexpected output for case: " + bench_case.name + " got='" + output
                                     + "' expected='" + bench_case.expected + "'");
        }
        samples.push_back(static_cast<double>(elapsed) / static_cast<double>(bench_case.iterations) / 1000.0);
    }

    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: bench_prebyte <root>\n";
        return 1;
    }

    const std::filesystem::path root = argv[1];

    prebyte::Prebyte simple_engine;
    simple_engine.set_variable("name", "Ada");

    prebyte::Prebyte conditional_engine;
    conditional_engine.set_variable("enabled", "true");

    prebyte::Prebyte include_engine;
    include_engine.set_variable("name", "Ada");
    include_engine.set_variable("enabled", "true");

    const std::vector<BenchCase> cases = {
        {
            .name = "simple-variable",
            .iterations = 20000,
            .render = [&]() { return simple_engine.process("Hello {{ name }}"); },
            .expected = "Hello Ada",
        },
        {
            .name = "conditional",
            .iterations = 20000,
            .render = [&]() { return conditional_engine.process("{{ if enabled }}Enabled{{ else }}Disabled{{ endif }}"); },
            .expected = "Enabled",
        },
        {
            .name = "include-if",
            .iterations = 5000,
            .render = [&]() {
                return include_engine.process_file((root / "tools/benchmark_compare/cases/prebyte/include_if/main.txt").string());
            },
            .expected = "Header for Ada\n\nEnabled\nFooter\n",
        },
    };

    for (const BenchCase& bench_case : cases) {
        std::cout << bench_case.name << '\t' << benchmark_case(bench_case) << '\n';
    }
    return 0;
}
