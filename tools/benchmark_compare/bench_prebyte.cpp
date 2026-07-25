#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "PrebyteEngine.h"
#include "app/BatchProcessor.h"
#include "app/Command.h"
#include "datatypes/Data.h"
#include "config/ProfileMerger.h"
#include "config/RuleResolver.h"
#include "config/VariableDefinitionParser.h"
#include "io/InputBuffer.h"
#include "io/InputReader.h"
#include "parser/JsonParser.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/IncludeResolver.h"
#include "runtime/Renderer.h"
#include "runtime/RenderSession.h"
#include "runtime/VariableStore.h"

namespace {

using Clock = std::chrono::steady_clock;

struct ManifestConfig {
    std::unordered_map<std::string, std::size_t> render_iterations;
    std::unordered_map<std::string, std::size_t> batch_iterations;
};

std::size_t data_as_size(const prebyte::Data& value) {
    if (value.is_int()) {
        return static_cast<std::size_t>(value.as_int());
    }
    if (value.is_double()) {
        return static_cast<std::size_t>(value.as_double());
    }
    throw std::runtime_error("manifest value must be numeric");
}

ManifestConfig load_manifest(const std::filesystem::path& root) {
    prebyte::JsonParser parser;
    const prebyte::Data manifest = parser.parse(root / "tools/benchmark_compare/manifest.json");
    const auto& manifest_map = manifest.as_map();

    ManifestConfig config;
    for (const prebyte::Data& case_data : manifest_map.at("render").as_map().at("cases").as_array()) {
        const auto& case_map = case_data.as_map();
        config.render_iterations.emplace(case_map.at("name").as_string(),
                                         data_as_size(case_map.at("iterations")));
    }
    for (const prebyte::Data& case_data : manifest_map.at("batch").as_map().at("cases").as_array()) {
        const auto& case_map = case_data.as_map();
        config.batch_iterations.emplace(case_map.at("name").as_string(),
                                        data_as_size(case_map.at("iterations")));
    }
    return config;
}

bool case_selected(std::string_view filter, std::string_view case_name) {
    if (filter.empty()) {
        return true;
    }
    const std::size_t separator = filter.find(':');
    if (separator == std::string_view::npos) {
        return filter == case_name;
    }
    return filter.substr(separator + 1) == case_name;
}

bool metric_selected(std::string_view filter, std::string_view mode, std::string_view case_name) {
    if (filter.empty()) {
        return true;
    }
    const std::size_t separator = filter.find(':');
    if (separator == std::string_view::npos) {
        return filter == case_name;
    }
    return filter.substr(0, separator) == mode && filter.substr(separator + 1) == case_name;
}

bool batch_metric_selected(std::string_view filter, std::string_view mode, std::string_view case_name) {
    if (filter.empty()) {
        return true;
    }
    const std::size_t separator = filter.find(':');
    if (separator == std::string_view::npos) {
        return filter == case_name;
    }
    return filter.substr(0, separator) == mode && filter.substr(separator + 1) == case_name;
}

struct BenchCase {
    std::string name;
    std::size_t iterations = 0;
    std::function<std::string()> render_cold;
    std::function<std::string()> render_warm_execute;
    std::function<std::string()> render_warm_memoized;
    std::function<std::string()> render_mt_warm_execute;
    std::function<std::string()> render_mt_warm_memoized;
    std::string expected;
};

struct WarmRenderState {
    WarmRenderState()
        : expression_engine(builtins),
          renderer(rule_resolver, include_resolver, expression_engine) {}

    static std::unique_ptr<WarmRenderState> inline_source(std::string_view source,
                                                          std::initializer_list<std::pair<std::string, std::string>> variables_init,
                                                          const std::function<void(WarmRenderState&)>& setup = {}) {
        auto state = std::make_unique<WarmRenderState>();
        for (const auto& [name, value] : variables_init) {
            state->variables.set(name, value);
        }
        state->settings = state->rule_resolver.resolve_for_file(state->configuration, {});
        prebyte::CompiledTemplateCompiler compiler;
        state->program = compiler.compile_source(source, {}, {}, state->settings);
        if (setup) {
            setup(*state);
        }
        return state;
    }

    static std::unique_ptr<WarmRenderState> file_source(const std::filesystem::path& path,
                                                        std::initializer_list<std::pair<std::string, std::string>> variables_init,
                                                        const std::function<void(WarmRenderState&)>& setup = {}) {
        auto state = std::make_unique<WarmRenderState>();
        for (const auto& [name, value] : variables_init) {
            state->variables.set(name, value);
        }

        state->current_file = path;
        state->settings = state->rule_resolver.resolve_for_file(state->configuration, path);
        state->effective_settings_cache[path] = state->settings;

        prebyte::CompiledTemplateSerializer serializer;
        if (const prebyte::CompiledProgram* compiled = serializer.try_load_valid(serializer.compiled_path_for_source(path), state->settings)) {
            state->program = *compiled;
        } else {
            const prebyte::InputBuffer input = prebyte::InputBuffer::from_file(path);
            prebyte::CompiledTemplateCompiler compiler;
            state->program = compiler.compile_source(input.view(), path, path, state->settings);
        }
        if (!state->program.logical_path.empty()) {
            state->current_file = state->program.logical_path;
            state->effective_settings_cache[state->current_file] = state->settings;
        }
        if (setup) {
            setup(*state);
        }
        static_cast<void>(state->render());
        return state;
    }

    std::string render() {
        render_session.reset_for_render();
        render_session.configuration_ref = &configuration;
        render_session.variables_ref = &variables;
        render_session.ignore_names_ref = &ignore_names;
        render_session.effective_settings_cache_ref = &effective_settings_cache;
        render_session.prepared_include_cache_ref = &prepared_include_cache;
        render_session.start_time = std::chrono::steady_clock::now();
        return renderer.render_program(program, settings, current_file, render_session);
    }

    prebyte::ResolvedConfiguration configuration;
    prebyte::VariableStore variables;
    std::set<std::string> ignore_names;
    prebyte::RuleResolver rule_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator expression_engine;
    prebyte::IncludeResolver include_resolver;
    prebyte::Renderer renderer;
    prebyte::EffectiveSettings settings;
    prebyte::CompiledProgram program;
    std::filesystem::path current_file;
    std::map<std::filesystem::path, prebyte::EffectiveSettings> effective_settings_cache;
    std::unordered_map<prebyte::RenderSession::PreparedIncludeKey, prebyte::RenderSession::PreparedIncludeEntry,
                       prebyte::RenderSession::PreparedIncludeKeyHash> prepared_include_cache;
    prebyte::RenderSession render_session;
};

std::vector<prebyte::Data::Map> load_batch_variable_sets(const std::filesystem::path& batch_data_path) {
    prebyte::JsonParser parser;
    const prebyte::Data root = parser.parse(batch_data_path);
    if (!root.is_array()) {
        throw std::runtime_error("Batch benchmark data must be a JSON array");
    }

    std::vector<prebyte::Data::Map> entries;
    entries.reserve(root.as_array().size());
    for (const prebyte::Data& item : root.as_array()) {
        if (!item.is_map()) {
            throw std::runtime_error("Batch benchmark entries must be JSON objects");
        }
        entries.push_back(item.as_map());
    }
    if (entries.empty()) {
        throw std::runtime_error("Batch benchmark data must contain at least one entry");
    }
    return entries;
}

std::string render_batch_entry(WarmRenderState& state, const prebyte::Data::Map& variables) {
    for (const auto& [name, value] : variables) {
        if (value.is_map() || value.is_array()) {
            state.variables.set_value(name, prebyte::Value::from_data(value));
        } else {
            state.variables.set(name, value.is_null() ? std::string() : value.as_string());
        }
    }
    return state.render();
}

std::string render_batch_entries(WarmRenderState& state, const std::vector<prebyte::Data::Map>& entries) {
    std::string output;
    output.reserve(entries.size() * 16);
    for (const prebyte::Data::Map& entry : entries) {
        output += render_batch_entry(state, entry);
    }
    return output;
}

std::string expected_batch_output(const std::vector<prebyte::Data::Map>& entries) {
    std::string output;
    output.reserve(entries.size() * 16);
    for (const prebyte::Data::Map& entry : entries) {
        const auto greeting = entry.find("greeting");
        const auto name = entry.find("name");
        if (greeting == entry.end() || name == entry.end()) {
            throw std::runtime_error("Batch benchmark entries require greeting and name");
        }
        output += greeting->second.as_string();
        output.push_back(' ');
        output += name->second.as_string();
        output.append("!\n");
    }
    return output;
}

double benchmark_batch_case(const std::function<std::string()>& render, const std::string& expected,
                            std::size_t iterations, std::size_t entries_per_iteration) {
    std::vector<double> samples;
    samples.reserve(5);
    for (int run = 0; run < 5; ++run) {
        std::string output;
        const auto start = Clock::now();
        for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
            output = render();
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
        if (output != expected) {
            throw std::runtime_error("Unexpected batch output got='" + output + "' expected='" + expected + "'");
        }
        const double total_renders = static_cast<double>(iterations) * static_cast<double>(entries_per_iteration);
        samples.push_back(static_cast<double>(elapsed) / total_renders / 1000.0);
    }

    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

prebyte::Value control_flow_members_value() {
    prebyte::Data::Array members;
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Ada")}, {"admin", prebyte::Data(true)}}));
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Grace")}, {"admin", prebyte::Data(false)}}));
    return prebyte::Value::list(std::move(members));
}

template <typename Factory>
std::function<std::string()> make_thread_local_warm_render(Factory factory) {
    return [factory = std::move(factory)]() mutable -> std::string {
        thread_local std::unique_ptr<WarmRenderState> state;
        if (!state) {
            state = factory();
        }
        return state->render();
    };
}

template <typename InitFn, typename RenderFn>
std::function<std::string()> make_thread_local_memoized_render(InitFn init, RenderFn render) {
    return [init = std::move(init), render = std::move(render)]() mutable -> std::string {
        thread_local prebyte::Prebyte engine;
        thread_local bool initialized = false;
        thread_local bool primed = false;
        if (!initialized) {
            init(engine);
            initialized = true;
        }
        if (!primed) {
            static_cast<void>(render(engine));
            primed = true;
        }
        return render(engine);
    };
}

double benchmark_case(const std::function<std::string()>& render, const BenchCase& bench_case) {
    std::vector<double> samples;
    samples.reserve(5);
    for (int run = 0; run < 5; ++run) {
        std::string output;
        const auto start = Clock::now();
        for (std::size_t iteration = 0; iteration < bench_case.iterations; ++iteration) {
            output = render();
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

std::size_t benchmark_threads() {
    const std::size_t hardware = std::max<std::size_t>(2, std::thread::hardware_concurrency());
    return hardware;
}

double benchmark_case_parallel(const std::function<std::string()>& render, const BenchCase& bench_case,
                               std::size_t threads) {
    std::vector<double> samples;
    samples.reserve(5);
    for (int run = 0; run < 5; ++run) {
        std::string output;
        std::string first_error;
        std::mutex sync_mutex;
        const auto start = Clock::now();
        std::vector<std::thread> workers;
        workers.reserve(threads);
        const std::size_t iterations_per_thread = bench_case.iterations / threads;
        const std::size_t remainder = bench_case.iterations % threads;
        for (std::size_t thread_index = 0; thread_index < threads; ++thread_index) {
            std::size_t thread_iterations = iterations_per_thread;
            if (thread_index == 0) {
                thread_iterations += remainder;
            }
            workers.emplace_back([&, thread_iterations]() {
                try {
                    std::string local_output;
                    for (std::size_t iteration = 0; iteration < thread_iterations; ++iteration) {
                        local_output = render();
                    }
                    std::lock_guard lock(sync_mutex);
                    output = std::move(local_output);
                } catch (const std::exception& error) {
                    std::lock_guard lock(sync_mutex);
                    if (first_error.empty()) {
                        first_error = error.what();
                    }
                }
            });
        }
        for (std::thread& worker : workers) {
            worker.join();
        }
        if (!first_error.empty()) {
            throw std::runtime_error(first_error);
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
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: bench_prebyte <root> [mode:case|case]\n";
        return 1;
    }

    const std::filesystem::path root = argv[1];
    const std::string_view filter = argc == 3 ? std::string_view(argv[2]) : std::string_view();
    const ManifestConfig manifest = load_manifest(root);
    const std::filesystem::path include_main = root / "tools/benchmark_compare/cases/prebyte/include_if/main.txt";
    const std::filesystem::path control_flow_members = root / "tools/benchmark_compare/cases/prebyte/control_flow/members.json";
    const std::filesystem::path batch_template = root / "tools/benchmark_compare/cases/batch/template.txt";
    const std::filesystem::path batch_data = root / "tools/benchmark_compare/cases/batch/data.json";
    const std::string control_flow_source =
        "{{ if members }}{{ for member in members }}{{ if member.admin }}*{{ else }}-{{ endif }}{{ member.name }};{{ endfor }}{{ elseif archived }}archived{{ else }}empty{{ endif }}";

    std::unique_ptr<WarmRenderState> warm_simple;
    if (case_selected(filter, "simple-variable")) {
        warm_simple = WarmRenderState::inline_source("Hello {{ name }}", {{"name", "Ada"}});
    }

    std::unique_ptr<WarmRenderState> warm_conditional;
    if (case_selected(filter, "conditional")) {
        warm_conditional = WarmRenderState::inline_source("{{ if enabled }}Enabled{{ else }}Disabled{{ endif }}",
                                                          {{"enabled", "true"}});
    }

    std::unique_ptr<WarmRenderState> warm_include;
    if (case_selected(filter, "include-if")) {
        warm_include = WarmRenderState::file_source(include_main, {{"name", "Ada"}, {"enabled", "true"}});
    }

    std::unique_ptr<WarmRenderState> warm_control_flow;
    if (case_selected(filter, "control-flow")) {
        warm_control_flow = WarmRenderState::inline_source(control_flow_source, {{"archived", "false"}}, [](WarmRenderState& state) {
            state.variables.set_value("members", control_flow_members_value());
        });
    }

    std::vector<BenchCase> cases;
    cases.reserve(4);
    if (case_selected(filter, "simple-variable")) {
        cases.push_back({
            .name = "simple-variable",
            .iterations = manifest.render_iterations.at("simple-variable"),
            .render_cold = [&]() {
                prebyte::Prebyte engine;
                engine.set_variable("name", "Ada");
                return engine.process("Hello {{ name }}");
            },
            .render_warm_execute = [&]() { return warm_simple->render(); },
            .render_warm_memoized = [&]() {
                static prebyte::Prebyte engine;
                static bool initialized = false;
                if (!initialized) {
                    engine.set_variable("name", "Ada");
                    initialized = true;
                }
                static bool primed = false;
                if (!primed) {
                    static_cast<void>(engine.process("Hello {{ name }}"));
                    primed = true;
                }
                return engine.process("Hello {{ name }}");
            },
            .render_mt_warm_execute = make_thread_local_warm_render([]() {
                return WarmRenderState::inline_source("Hello {{ name }}", {{"name", "Ada"}});
            }),
            .render_mt_warm_memoized = make_thread_local_memoized_render(
                [](prebyte::Prebyte& engine) { engine.set_variable("name", "Ada"); },
                [](prebyte::Prebyte& engine) { return engine.process("Hello {{ name }}"); }),
            .expected = "Hello Ada",
        });
    }

    if (case_selected(filter, "conditional")) {
        cases.push_back({
            .name = "conditional",
            .iterations = manifest.render_iterations.at("conditional"),
            .render_cold = [&]() {
                prebyte::Prebyte engine;
                engine.set_variable("enabled", "true");
                return engine.process("{{ if enabled }}Enabled{{ else }}Disabled{{ endif }}");
            },
            .render_warm_execute = [&]() { return warm_conditional->render(); },
            .render_warm_memoized = [&]() {
                static prebyte::Prebyte engine;
                static bool initialized = false;
                if (!initialized) {
                    engine.set_variable("enabled", "true");
                    initialized = true;
                }
                static bool primed = false;
                if (!primed) {
                    static_cast<void>(engine.process("{{ if enabled }}Enabled{{ else }}Disabled{{ endif }}"));
                    primed = true;
                }
                return engine.process("{{ if enabled }}Enabled{{ else }}Disabled{{ endif }}");
            },
            .render_mt_warm_execute = make_thread_local_warm_render([]() {
                return WarmRenderState::inline_source("{{ if enabled }}Enabled{{ else }}Disabled{{ endif }}",
                                                      {{"enabled", "true"}});
            }),
            .render_mt_warm_memoized = make_thread_local_memoized_render(
                [](prebyte::Prebyte& engine) { engine.set_variable("enabled", "true"); },
                [](prebyte::Prebyte& engine) {
                    return engine.process("{{ if enabled }}Enabled{{ else }}Disabled{{ endif }}");
                }),
            .expected = "Enabled",
        });
    }

    if (case_selected(filter, "include-if")) {
        cases.push_back({
            .name = "include-if",
            .iterations = manifest.render_iterations.at("include-if"),
            .render_cold = [&]() {
                prebyte::Prebyte engine;
                engine.set_variable("name", "Ada");
                engine.set_variable("enabled", "true");
                return engine.process_file(include_main.string());
            },
            .render_warm_execute = [&]() { return warm_include->render(); },
            .render_warm_memoized = [&]() {
                static prebyte::Prebyte engine;
                static bool initialized = false;
                if (!initialized) {
                    engine.set_variable("name", "Ada");
                    engine.set_variable("enabled", "true");
                    initialized = true;
                }
                static bool primed = false;
                if (!primed) {
                    static_cast<void>(engine.process_file(include_main.string()));
                    primed = true;
                }
                return engine.process_file(include_main.string());
            },
            .render_mt_warm_execute = make_thread_local_warm_render([&include_main]() {
                return WarmRenderState::file_source(include_main, {{"name", "Ada"}, {"enabled", "true"}});
            }),
            .render_mt_warm_memoized = make_thread_local_memoized_render(
                [](prebyte::Prebyte& engine) {
                    engine.set_variable("name", "Ada");
                    engine.set_variable("enabled", "true");
                },
                [&include_main](prebyte::Prebyte& engine) { return engine.process_file(include_main.string()); }),
            .expected = "Header for Ada\n\nEnabled\nFooter\n",
        });
    }

    if (case_selected(filter, "control-flow")) {
        cases.push_back({
            .name = "control-flow",
            .iterations = manifest.render_iterations.at("control-flow"),
            .render_cold = [&]() {
                prebyte::Prebyte engine;
                engine.set_variable("archived", "false");
                engine.set_variable("members", "@" + control_flow_members.string());
                return engine.process(control_flow_source);
            },
            .render_warm_execute = [&]() { return warm_control_flow->render(); },
            .render_warm_memoized = [&]() {
                static prebyte::Prebyte engine;
                static bool initialized = false;
                if (!initialized) {
                    engine.set_variable("archived", "false");
                    engine.set_variable("members", "@" + control_flow_members.string());
                    initialized = true;
                }
                static bool primed = false;
                if (!primed) {
                    static_cast<void>(engine.process(control_flow_source));
                    primed = true;
                }
                return engine.process(control_flow_source);
            },
            .render_mt_warm_execute = make_thread_local_warm_render([&control_flow_source, &control_flow_members]() {
                return WarmRenderState::inline_source(control_flow_source, {{"archived", "false"}},
                                                       [](WarmRenderState& state) {
                                                           state.variables.set_value("members", control_flow_members_value());
                                                       });
            }),
            .render_mt_warm_memoized = make_thread_local_memoized_render(
                [&control_flow_members](prebyte::Prebyte& engine) {
                    engine.set_variable("archived", "false");
                    engine.set_variable("members", "@" + control_flow_members.string());
                },
                [&control_flow_source](prebyte::Prebyte& engine) { return engine.process(control_flow_source); }),
            .expected = "*Ada;-Grace;",
        });
    }

    if (cases.empty() && !case_selected(filter, "batch-variable")) {
        std::cerr << "No benchmark case matched filter: " << filter << '\n';
        return 1;
    }

    const std::size_t threads = benchmark_threads();
    for (const BenchCase& bench_case : cases) {
        if (metric_selected(filter, "cold", bench_case.name)) {
            std::cout << "cold:" << bench_case.name << '\t' << benchmark_case(bench_case.render_cold, bench_case) << '\n';
        }
        if (metric_selected(filter, "warm-execute", bench_case.name)) {
            std::cout << "warm-execute:" << bench_case.name << '\t' << benchmark_case(bench_case.render_warm_execute, bench_case) << '\n';
        }
        if (metric_selected(filter, "warm-memoized", bench_case.name)) {
            std::cout << "warm-memoized:" << bench_case.name << '\t' << benchmark_case(bench_case.render_warm_memoized, bench_case) << '\n';
        }
        if (metric_selected(filter, "mt-cold", bench_case.name)) {
            std::cout << "mt-cold:" << bench_case.name << '\t'
                      << benchmark_case_parallel(bench_case.render_cold, bench_case, threads) << '\n';
        }
        if (metric_selected(filter, "mt-warm-execute", bench_case.name)) {
            std::cout << "mt-warm-execute:" << bench_case.name << '\t'
                      << benchmark_case_parallel(bench_case.render_mt_warm_execute, bench_case, threads) << '\n';
        }
        if (metric_selected(filter, "mt-warm-memoized", bench_case.name)) {
            std::cout << "mt-warm-memoized:" << bench_case.name << '\t'
                      << benchmark_case_parallel(bench_case.render_mt_warm_memoized, bench_case, threads) << '\n';
        }
    }

    if (case_selected(filter, "batch-variable")) {
        const std::vector<prebyte::Data::Map> batch_entries = load_batch_variable_sets(batch_data);
        const std::string batch_expected = expected_batch_output(batch_entries);
        const std::size_t batch_iterations = manifest.batch_iterations.at("batch-variable");
        const std::size_t entries_per_iteration = batch_entries.size();

        prebyte::Command batch_command;
        batch_command.mode = prebyte::CommandMode::Render;
        batch_command.input_path = batch_template;
        batch_command.batch_path = batch_data;

        std::unique_ptr<WarmRenderState> warm_batch = WarmRenderState::file_source(batch_template, {});

        if (batch_metric_selected(filter, "batch-warm", "batch-variable")) {
            const double micros = benchmark_batch_case(
                [&]() { return render_batch_entries(*warm_batch, batch_entries); },
                batch_expected,
                batch_iterations,
                entries_per_iteration);
            std::cout << "batch-warm:batch-variable\t" << micros << '\n';
        }
        if (batch_metric_selected(filter, "batch-cold", "batch-variable")) {
            const double micros = benchmark_batch_case(
                [&]() {
                    prebyte::BatchProcessor processor;
                    return processor.execute(batch_command);
                },
                batch_expected,
                batch_iterations,
                entries_per_iteration);
            std::cout << "batch-cold:batch-variable\t" << micros << '\n';
        }
        if (batch_metric_selected(filter, "sequential-warm", "batch-variable")) {
            const double micros = benchmark_batch_case(
                [&]() { return render_batch_entries(*warm_batch, batch_entries); },
                batch_expected,
                batch_iterations,
                entries_per_iteration);
            std::cout << "sequential-warm:batch-variable\t" << micros << '\n';
        }
        if (batch_metric_selected(filter, "sequential-cold", "batch-variable")) {
            const double micros = benchmark_batch_case(
                [&]() {
                    std::string output;
                    output.reserve(batch_expected.size());
                    for (const prebyte::Data::Map& entry : batch_entries) {
                        prebyte::Prebyte engine;
                        for (const auto& [name, value] : entry) {
                            engine.set_variable(name, value.is_null() ? std::string() : value.as_string());
                        }
                        output += engine.process_file(batch_template.string());
                    }
                    return output;
                },
                batch_expected,
                batch_iterations,
                entries_per_iteration);
            std::cout << "sequential-cold:batch-variable\t" << micros << '\n';
        }
    }

    return 0;
}
