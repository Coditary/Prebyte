#include "TestHarness.h"

#include "PrebyteEngine.h"
#include "app/AppRunner.h"
#include "app/Command.h"
#include "runtime/compiled/CompiledTemplateCompiler.h"
#include "runtime/compiled/CompiledTemplateSerializer.h"
#include "runtime/cache/FileMetadataCache.h"
#include "support/Diagnostic.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>
#include <vector>

namespace {

constexpr int kThreadCount = 8;
constexpr int kRendersPerThread = 100;

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path concurrency_test_root(const std::string& name) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "prebyte-concurrency-e2e" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void reset_template_caches() {
    prebyte::FileMetadataCache::instance().clear();
}

std::string render_fixture_with_app_runner(const std::filesystem::path& input_path,
                                           const std::vector<std::string>& define_args = {}) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = input_path;
    command.define_args = define_args;

    prebyte::AppRunner runner;
    return runner.execute(command);
}

void run_parallel_app_runner_jobs(const std::function<std::string()>& job) {
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    std::atomic<int> failures{0};

    for (int thread_index = 0; thread_index < kThreadCount; ++thread_index) {
        threads.emplace_back([&job, &failures]() {
            for (int render_index = 0; render_index < kRendersPerThread; ++render_index) {
                try {
                    static_cast<void>(job());
                } catch (const std::exception&) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    REQUIRE_EQ(failures.load(), 0);
}

void run_parallel_prebyte_jobs(prebyte::Prebyte& engine, const std::function<std::string()>& job) {
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    std::atomic<int> failures{0};

    for (int thread_index = 0; thread_index < kThreadCount; ++thread_index) {
        threads.emplace_back([&engine, &job, &failures]() {
            for (int render_index = 0; render_index < kRendersPerThread; ++render_index) {
                try {
                    static_cast<void>(job());
                } catch (const std::exception&) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    REQUIRE_EQ(failures.load(), 0);
}

}

TEST_CASE(ConcurrencyE2E_app_runner_parallel_fixture_render_with_includes_is_stable) {
    reset_template_caches();

    const std::filesystem::path input_path = "tests/fixtures/render_include_if/input.txt";
    const std::string expected = render_fixture_with_app_runner(input_path, {"name=Ada", "enabled=true"});

    run_parallel_app_runner_jobs([&]() {
        const std::string output = render_fixture_with_app_runner(input_path, {"name=Ada", "enabled=true"});
        if (output != expected) {
            throw std::runtime_error("unexpected render output");
        }
        return output;
    });
}

TEST_CASE(ConcurrencyE2E_app_runner_parallel_file_render_with_nested_includes_is_stable) {
    reset_template_caches();

    const std::filesystem::path root = concurrency_test_root("nested-includes");
    write_file(root / "header.txt", "Header {{ name }}\n");
    write_file(root / "body.pbt", "{{ include \"header.txt\" }}\n{{ for item in items }}<{{ item }}>{{ endfor }}\n");
    write_file(root / "main.pbt", "{{ include \"body.pbt\" }}Footer\n");
    write_file(root / "items.yaml", "- Ada\n- Grace\n");

    const std::string expected = render_fixture_with_app_runner(
        root / "main.pbt",
        {"name=Ada", "items=@" + (root / "items.yaml").string()});
    REQUIRE(expected.find("Header Ada") != std::string::npos);
    REQUIRE(expected.find("<Ada><Grace>") != std::string::npos);
    REQUIRE(expected.find("Footer") != std::string::npos);

    run_parallel_app_runner_jobs([&]() {
        const std::string output = render_fixture_with_app_runner(
            root / "main.pbt",
            {"name=Ada", "items=@" + (root / "items.yaml").string()});
        if (output != expected) {
            throw std::runtime_error("unexpected render output");
        }
        return output;
    });
}

TEST_CASE(ConcurrencyE2E_app_runner_parallel_render_shares_adjacent_pbc_cache) {
    reset_template_caches();

    const std::filesystem::path root = concurrency_test_root("adjacent-pbc");
    const std::filesystem::path source_path = root / "main.pbt";
    const std::filesystem::path logical_path = root / "main";
    write_file(source_path, "Hello {{ name }} from {{ config.server.host }}\n");
    write_file(root / "config.toml", "[server]\nhost=\"localhost\"\n");

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program =
        compiler.compile_source("Hello {{ name }} from {{ config.server.host }}\n", source_path, logical_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(serializer.compiled_path_for_source(source_path), serializer.serialize(program));

    const std::string expected =
        render_fixture_with_app_runner(source_path, {"name=Ada", "config=@" + (root / "config.toml").string()});

    run_parallel_app_runner_jobs([&]() {
        const std::string output =
            render_fixture_with_app_runner(source_path, {"name=Ada", "config=@" + (root / "config.toml").string()});
        if (output != expected) {
            throw std::runtime_error("unexpected render output");
        }
        return output;
    });
}

TEST_CASE(ConcurrencyE2E_app_runner_parallel_structured_import_render_is_stable) {
    reset_template_caches();

    const std::filesystem::path root = concurrency_test_root("structured-imports");
    write_file(root / "user.json", R"({"name":"Ada"})");
    write_file(root / "items.yaml", "- Ada\n- Grace\n");
    write_file(root / "main.pbt", "{{ user.name }}|{{ items[1] }}\n");

    const std::string expected = render_fixture_with_app_runner(
        root / "main.pbt",
        {"user=@" + (root / "user.json").string(), "items=@" + (root / "items.yaml").string()});
    REQUIRE_EQ(expected, std::string("Ada|Grace\n"));

    run_parallel_app_runner_jobs([&]() {
        const std::string output = render_fixture_with_app_runner(
            root / "main.pbt",
            {"user=@" + (root / "user.json").string(), "items=@" + (root / "items.yaml").string()});
        if (output != expected) {
            throw std::runtime_error("unexpected render output");
        }
        return output;
    });
}

TEST_CASE(ConcurrencyE2E_prebyte_engine_parallel_process_file_with_includes_is_stable) {
    reset_template_caches();

    const std::filesystem::path root = concurrency_test_root("prebyte-file");
    write_file(root / "header.txt", "Header {{ name }}\n");
    write_file(root / "main.pbt", "{{ include \"header.txt\" }}\n{{ if enabled }}Enabled{{ else }}Disabled{{ endif }}\nFooter\n");

    prebyte::Prebyte engine;
    engine.set_variable("name", "Ada");
    engine.set_variable("enabled", "true");

    const std::string expected = engine.process_file((root / "main.pbt").string());
    REQUIRE_EQ(expected, std::string("Header Ada\n\nEnabled\nFooter\n"));

    run_parallel_prebyte_jobs(engine, [&]() {
        const std::string output = engine.process_file((root / "main.pbt").string());
        if (output != expected) {
            throw std::runtime_error("unexpected render output");
        }
        return output;
    });
}

TEST_CASE(ConcurrencyE2E_mixed_app_runner_and_prebyte_parallel_renders_are_stable) {
    reset_template_caches();

    const std::filesystem::path root = concurrency_test_root("mixed");
    write_file(root / "main.pbt", "Mixed {{ name }} {{ lua \"return upper(name)\" }}\n");

    prebyte::Prebyte engine;
    engine.set_variable("name", "Ada");
    const std::string expected = engine.process_file((root / "main.pbt").string());
    REQUIRE_EQ(expected, std::string("Mixed Ada ADA\n"));

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    std::atomic<int> failures{0};

    for (int thread_index = 0; thread_index < kThreadCount; ++thread_index) {
        threads.emplace_back([&, thread_index]() {
            for (int render_index = 0; render_index < kRendersPerThread; ++render_index) {
                try {
                    std::string output;
                    if (thread_index % 2 == 0) {
                        prebyte::Command command;
                        command.mode = prebyte::CommandMode::Render;
                        command.input_path = root / "main.pbt";
                        command.define_args = {"name=Ada"};
                        prebyte::AppRunner runner;
                        output = runner.execute(command);
                    } else {
                        output = engine.process_file((root / "main.pbt").string());
                    }
                    if (output != expected) {
                        failures.fetch_add(1, std::memory_order_relaxed);
                    }
                } catch (const std::exception&) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    REQUIRE_EQ(failures.load(), 0);
}
