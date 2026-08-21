#include "TestHarness.h"

#include "app/AppRunner.h"
#include "app/Command.h"
#include "config/RuleResolver.h"
#include "datatypes/Data.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/CompiledTemplateCache.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/FileMetadataCache.h"
#include "runtime/IncludeResolver.h"
#include "runtime/Renderer.h"
#include "support/Diagnostic.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

namespace {

constexpr std::uint32_t kPropertySeedBase = 0x50726F70; // "Prop"

struct PropertyHarness {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator{builtins};
    prebyte::Renderer renderer{rule_resolver, include_resolver, evaluator};
    prebyte::CompiledTemplateCompiler compiler;
    prebyte::CompiledTemplateSerializer serializer;
};

std::size_t property_iterations() {
    if (const char* env = std::getenv("PREBYTE_PBT_ITERATIONS")) {
        return static_cast<std::size_t>(std::stoul(env));
    }
    return 250;
}

[[noreturn]] void property_fail(std::size_t iteration, std::string_view property, const std::string& source,
                                const std::string& detail) {
    std::ostringstream stream;
    stream << "property=" << property << " iteration=" << iteration << ' ' << detail << " template='" << source
           << '\'';
    throw prebyte::test::AssertionFailure(stream.str());
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path property_test_root(const std::string& name) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "prebyte-render-property-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

std::string random_token(std::mt19937& rng, std::size_t max_len) {
    static constexpr char kChars[] = "abcdefghijklmnopqrstuvwxyz0123456789_-";
    std::uniform_int_distribution<int> length_dist(1, static_cast<int>(max_len));
    std::uniform_int_distribution<int> char_dist(0, static_cast<int>(sizeof(kChars) - 2));
    const int length = length_dist(rng);
    std::string token;
    token.reserve(static_cast<std::size_t>(length));
    for (int index = 0; index < length; ++index) {
        token.push_back(kChars[char_dist(rng)]);
    }
    return token;
}

prebyte::RenderSession make_property_session(std::mt19937& rng) {
    prebyte::RenderSession session;
    session.variables.set("name", random_token(rng, 12));
    session.variables.set("enabled", (rng() % 2) == 0 ? "true" : "false");
    session.variables.set("count", std::to_string(rng() % 100));

    prebyte::Data::Array items;
    const std::size_t item_count = 1 + (rng() % 4);
    for (std::size_t index = 0; index < item_count; ++index) {
        items.push_back(prebyte::Data(random_token(rng, 8)));
    }
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));
    return session;
}

class TemplateGenerator {
public:
    explicit TemplateGenerator(std::mt19937& rng, bool allow_includes = false) : rng_(rng), allow_includes_(allow_includes) {}

    std::string generate() { return generate_block(3); }

private:
    int pick(int upper_exclusive) {
        std::uniform_int_distribution<int> dist(0, upper_exclusive - 1);
        return dist(rng_);
    }

    std::string generate_literal() {
        static constexpr char kChars[] = " abcdefghijklmnopqrstuvwxyz\n\t-|";
        std::uniform_int_distribution<int> length_dist(0, 10);
        std::uniform_int_distribution<int> char_dist(0, static_cast<int>(sizeof(kChars) - 2));
        const int length = length_dist(rng_);
        std::string literal;
        literal.reserve(static_cast<std::size_t>(length));
        for (int index = 0; index < length; ++index) {
            literal.push_back(kChars[char_dist(rng_)]);
        }
        return literal;
    }

    std::string variable_reference() {
        switch (pick(6)) {
        case 0:
            return "name";
        case 1:
            return "enabled";
        case 2:
            return "count";
        case 3:
            return "items[0]";
        case 4:
            return "items[1]";
        default:
            return "item";
        }
    }

    std::string interpolation() {
        switch (pick(5)) {
        case 0:
            return "{{ " + variable_reference() + " }}";
        case 1:
            return "{{ " + variable_reference() + " | upper }}";
        case 2:
            return "{{ " + variable_reference() + " | lower }}";
        case 3:
            return "{{ len(items) }}";
        default:
            return "{{ len(name) }}";
        }
    }

    std::string generate_block(int depth) {
        if (depth <= 0) {
            return pick(2) == 0 ? generate_literal() : interpolation();
        }

        switch (pick(allow_includes_ ? 7 : 6)) {
        case 0:
        case 1:
            return generate_literal();
        case 2:
            return interpolation();
        case 3:
            return "{{ if enabled }}" + generate_block(depth - 1) + "{{ else }}" + generate_block(depth - 1)
                   + "{{ endif }}";
        case 4:
            return "{{ for item in items }}" + generate_block(depth - 1) + "{{ else }}empty{{ endfor }}";
        case 5:
            return "{{ set tag = name }}" + generate_block(depth - 1) + "{{ tag }}";
        default:
            return "{{ include \"partial.pbt\" }}" + generate_block(depth - 1);
        }
    }

    std::mt19937& rng_;
    bool allow_includes_;
};

std::string render_direct(PropertyHarness& harness, const std::string& source,
                          const std::filesystem::path& current_file, prebyte::RenderSession& session,
                          const prebyte::EffectiveSettings& settings) {
    return harness.renderer.render_source(source, settings, current_file, session);
}

std::string render_program(PropertyHarness& harness, const prebyte::CompiledProgram& program,
                           prebyte::RenderSession& session, const prebyte::EffectiveSettings& settings) {
    return harness.renderer.render_program(program, settings, program.logical_path, session);
}

prebyte::CompiledProgram serialize_roundtrip(PropertyHarness& harness, const prebyte::CompiledProgram& program) {
    return harness.serializer.deserialize(harness.serializer.serialize(program));
}

std::string app_runner_render(const std::filesystem::path& input_path,
                              const std::filesystem::path& items_path) {
    prebyte::Command command;
    command.mode = prebyte::CommandMode::Render;
    command.input_path = input_path;
    command.define_args = {"name=Ada", "enabled=true", "count=7", "items=@" + items_path.string()};

    prebyte::AppRunner runner;
    return runner.execute(command);
}

}

TEST_CASE(RenderProperty_compile_serialize_roundtrip_matches_direct_render) {
    PropertyHarness harness;
    prebyte::EffectiveSettings settings;
    settings.strict_variables = false;
    const std::filesystem::path current_file = "property/main.pbt";
    const std::size_t iterations = property_iterations();

    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        std::mt19937 rng(kPropertySeedBase ^ static_cast<std::uint32_t>(iteration));
        TemplateGenerator generator(rng);
        const std::string source = generator.generate();
        prebyte::RenderSession session = make_property_session(rng);

        const std::string direct = render_direct(harness, source, current_file, session, settings);
        const prebyte::CompiledProgram compiled =
            harness.compiler.compile_source(source, current_file, current_file, settings);
        const prebyte::CompiledProgram roundtrip = serialize_roundtrip(harness, compiled);
        const std::string via_roundtrip = render_program(harness, roundtrip, session, settings);

        if (direct != via_roundtrip) {
            property_fail(iteration, "compile_serialize_roundtrip", source,
                          "direct='" + direct + "' roundtrip='" + via_roundtrip + "'");
        }
    }
}

TEST_CASE(RenderProperty_double_serialization_is_byte_stable) {
    PropertyHarness harness;
    prebyte::EffectiveSettings settings;
    const std::filesystem::path current_file = "property/bytes.pbt";
    const std::size_t iterations = property_iterations();

    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        std::mt19937 rng(kPropertySeedBase ^ static_cast<std::uint32_t>(iteration + 10000));
        TemplateGenerator generator(rng);
        const std::string source = generator.generate();

        const prebyte::CompiledProgram compiled =
            harness.compiler.compile_source(source, current_file, current_file, settings);
        const std::string once = harness.serializer.serialize(compiled);
        const prebyte::CompiledProgram loaded = harness.serializer.deserialize(once, current_file);
        const std::string twice = harness.serializer.serialize(loaded);

        if (once != twice) {
            property_fail(iteration, "double_serialization_bytes", source, "serialized bytes changed after roundtrip");
        }
    }
}

TEST_CASE(RenderProperty_render_is_idempotent_for_compiled_program) {
    PropertyHarness harness;
    prebyte::EffectiveSettings settings;
    const std::filesystem::path current_file = "property/idempotent.pbt";
    const std::size_t iterations = property_iterations();

    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        std::mt19937 rng(kPropertySeedBase ^ static_cast<std::uint32_t>(iteration + 20000));
        TemplateGenerator generator(rng);
        const std::string source = generator.generate();
        prebyte::RenderSession session = make_property_session(rng);

        const prebyte::CompiledProgram compiled =
            harness.compiler.compile_source(source, current_file, current_file, settings);
        const std::string first = render_program(harness, compiled, session, settings);
        const std::string second = render_program(harness, compiled, session, settings);

        if (first != second) {
            property_fail(iteration, "render_idempotent", source,
                          "first='" + first + "' second='" + second + "'");
        }
    }
}

TEST_CASE(RenderProperty_recompile_produces_same_render_output) {
    PropertyHarness harness;
    prebyte::EffectiveSettings settings;
    const std::filesystem::path current_file = "property/recompile.pbt";
    const std::size_t iterations = property_iterations();

    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        std::mt19937 rng(kPropertySeedBase ^ static_cast<std::uint32_t>(iteration + 30000));
        TemplateGenerator generator(rng);
        const std::string source = generator.generate();
        prebyte::RenderSession session = make_property_session(rng);

        const prebyte::CompiledProgram first =
            harness.compiler.compile_source(source, current_file, current_file, settings);
        const prebyte::CompiledProgram second =
            harness.compiler.compile_source(source, current_file, current_file, settings);
        const std::string first_output = render_program(harness, first, session, settings);
        const std::string second_output = render_program(harness, second, session, settings);

        if (first_output != second_output) {
            property_fail(iteration, "recompile_same_output", source,
                          "first='" + first_output + "' second='" + second_output + "'");
        }
    }
}

TEST_CASE(RenderProperty_include_templates_roundtrip_matches_direct_render) {
    PropertyHarness harness;
    const std::filesystem::path root = property_test_root("includes");
    const std::filesystem::path current_file = root / "main.pbt";
    write_file(root / "partial.pbt", "<{{ loop.index }}:{{ item }}>\n");

    prebyte::EffectiveSettings settings;
    settings.allow_includes = true;
    settings.include_paths.push_back(root);
    const std::size_t iterations = property_iterations();

    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        std::mt19937 rng(kPropertySeedBase ^ static_cast<std::uint32_t>(iteration + 40000));
        TemplateGenerator generator(rng, true);
        const std::string source = generator.generate();
        prebyte::RenderSession session = make_property_session(rng);

        const std::string direct = render_direct(harness, source, current_file, session, settings);
        const prebyte::CompiledProgram compiled =
            harness.compiler.compile_source(source, current_file, current_file, settings);
        const prebyte::CompiledProgram roundtrip = serialize_roundtrip(harness, compiled);
        const std::string via_roundtrip = render_program(harness, roundtrip, session, settings);

        if (direct != via_roundtrip) {
            property_fail(iteration, "include_roundtrip", source,
                          "direct='" + direct + "' roundtrip='" + via_roundtrip + "'");
        }
    }
}

TEST_CASE(RenderProperty_app_runner_source_matches_adjacent_pbc) {
    const std::filesystem::path root = property_test_root("app-runner-pbc");

    PropertyHarness harness;
    prebyte::EffectiveSettings settings;
    const std::size_t iterations = property_iterations() / 5;

    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        const std::filesystem::path iteration_root = root / std::to_string(iteration);
        const std::filesystem::path source_path = iteration_root / "main.pbt";
        write_file(iteration_root / "items.yaml", "- one\n- two\n");

        std::mt19937 rng(kPropertySeedBase ^ static_cast<std::uint32_t>(iteration + 50000));
        TemplateGenerator generator(rng);
        const std::string source = generator.generate();
        write_file(source_path, source);

        const prebyte::CompiledProgram compiled =
            harness.compiler.compile_source(source, source_path, source_path, settings);
        const std::filesystem::path compiled_path = harness.serializer.compiled_path_for_source(source_path);
        write_file(compiled_path, harness.serializer.serialize(compiled));

        prebyte::FileMetadataCache::instance().clear();
        prebyte::CompiledTemplateCache::instance().erase(compiled_path, settings);

        const std::string from_source = app_runner_render(source_path, iteration_root / "items.yaml");
        const std::string from_pbc = app_runner_render(compiled_path, iteration_root / "items.yaml");

        if (from_source != from_pbc) {
            property_fail(iteration, "app_runner_pbc_parity", source,
                          "source='" + from_source + "' pbc='" + from_pbc + "'");
        }
    }
}
