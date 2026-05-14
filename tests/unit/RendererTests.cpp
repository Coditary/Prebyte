#include "TestHarness.h"

#include "datatypes/Data.h"
#include "config/RuleResolver.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "runtime/IncludeResolver.h"
#include "runtime/Renderer.h"
#include "support/Diagnostic.h"

#include <functional>
#include <filesystem>
#include <fstream>

namespace {

void write_test_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path renderer_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-renderer-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void require_case_output(std::string_view label, const std::string& actual, std::string_view expected) {
    if (actual != expected) {
        throw ::prebyte::test::AssertionFailure(std::string(label) + " expected='" + std::string(expected)
                                                + "' actual='" + actual + "'");
    }
}

}

TEST_CASE(Renderer_replace_variable) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::ResolvedConfiguration configuration;
    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.configuration = configuration;
    session.variables.set("name", "Ada");

    const std::string output = renderer.render_source("Hello {{ name }}", settings, "inline", session);
    REQUIRE_EQ(output, std::string("Hello Ada"));
}

TEST_CASE(Renderer_render_source_to_matches_collected_output) {
    const std::filesystem::path root = renderer_test_root("stream-source");
    write_test_file(root / "partial.pbt", "<{{ loop.index }}:{{ item }}>");

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    const std::string source = "A{{ for item in items }}{{ include \"./partial\" }}{{ endfor }}Z";

    auto make_session = []() {
        prebyte::RenderSession session;
        prebyte::Data::Array items;
        items.push_back(prebyte::Data("Ada"));
        items.push_back(prebyte::Data("Grace"));
        session.variables.set_value("items", prebyte::Value::list(std::move(items)));
        return session;
    };

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession streamed_session = make_session();
    std::string streamed;
    std::size_t chunk_count = 0;
    renderer.render_source_to(source, settings, root / "main.pbt", streamed_session, [&](std::string_view chunk) {
        ++chunk_count;
        streamed.append(chunk.data(), chunk.size());
    });

    prebyte::RenderSession collected_session = make_session();
    const std::string collected = renderer.render_source(source, settings, root / "main.pbt", collected_session);

    REQUIRE_EQ(streamed, collected);
    REQUIRE(chunk_count > 1);
}

TEST_CASE(Renderer_control_flow_truthiness_matrix_across_value_types) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    const std::string if_source = "{{ if value }}if{{ else }}else{{ endif }}";
    const std::string elseif_source = "{{ if missing }}bad{{ elseif value }}elseif{{ else }}else{{ endif }}";

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram if_program = serializer.deserialize(serializer.serialize(
        compiler.compile_source(if_source, "inline", "inline", settings)));
    const prebyte::CompiledProgram elseif_program = serializer.deserialize(serializer.serialize(
        compiler.compile_source(elseif_source, "inline", "inline", settings)));

    struct TruthinessCase {
        std::string label;
        prebyte::Value value;
        bool truthy;
    };

    std::vector<TruthinessCase> cases;
    cases.push_back({"null", prebyte::Value(), false});
    cases.push_back({"bool-false", prebyte::Value(false), false});
    cases.push_back({"bool-true", prebyte::Value(true), true});
    cases.push_back({"number-zero", prebyte::Value(0.0), false});
    cases.push_back({"number-one", prebyte::Value(1.0), true});
    cases.push_back({"string-empty", prebyte::Value(std::string()), false});
    cases.push_back({"string-false", prebyte::Value(std::string("false")), false});
    cases.push_back({"string-value", prebyte::Value(std::string("Ada")), true});
    cases.push_back({"list-empty", prebyte::Value::list(prebyte::Data::Array{}), false});
    cases.push_back({"list-items", prebyte::Value::list(prebyte::Data::Array{prebyte::Data("Ada")}), true});
    cases.push_back({"object-empty", prebyte::Value::object(prebyte::Data::Map{}), false});
    cases.push_back({"object-items", prebyte::Value::object(prebyte::Data::Map{{"name", prebyte::Data("Ada")}}), true});

    for (const auto& truthiness_case : cases) {
        const std::string if_expected = truthiness_case.truthy ? "if" : "else";
        const std::string elseif_expected = truthiness_case.truthy ? "elseif" : "else";

        auto make_session = [&]() {
            prebyte::RenderSession session;
            session.variables.set_value("value", truthiness_case.value);
            return session;
        };

        prebyte::RenderSession if_source_session = make_session();
        require_case_output(truthiness_case.label + " if source",
                            renderer.render_source(if_source, settings, "inline", if_source_session),
                            if_expected);

        prebyte::RenderSession if_compiled_session = make_session();
        require_case_output(truthiness_case.label + " if compiled",
                            renderer.render_program(if_program, settings, if_program.logical_path, if_compiled_session),
                            if_expected);

        prebyte::RenderSession elseif_source_session = make_session();
        require_case_output(truthiness_case.label + " elseif source",
                            renderer.render_source(elseif_source, settings, "inline", elseif_source_session),
                            elseif_expected);

        prebyte::RenderSession elseif_compiled_session = make_session();
        require_case_output(truthiness_case.label + " elseif compiled",
                            renderer.render_program(elseif_program, settings, elseif_program.logical_path, elseif_compiled_session),
                            elseif_expected);
    }
}

TEST_CASE(Renderer_control_flow_source_and_compiled_parity_table) {
    const std::filesystem::path root = renderer_test_root("control-flow-parity-table");
    write_test_file(root / "if_partial.pbt", "Hello {{ name }}");
    write_test_file(root / "for_partial.pbt", "<{{ loop.index }}:{{ item }}>");

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);
    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    prebyte::CompiledTemplateSerializer serializer;

    struct ControlFlowCase {
        std::string label;
        std::string source;
        std::filesystem::path current_file;
        std::function<void(prebyte::RenderSession&)> setup;
        std::string expected;
    };

    std::vector<ControlFlowCase> cases;
    cases.push_back({
        .label = "if-else",
        .source = "{{ if enabled }}Y{{ else }}N{{ endif }}",
        .current_file = {},
        .setup = [](prebyte::RenderSession& session) { session.variables.set("enabled", "true"); },
        .expected = "Y",
    });
    cases.push_back({
        .label = "if-elseif-else",
        .source = "{{ if primary }}A{{ elseif secondary }}B{{ else }}C{{ endif }}",
        .current_file = {},
        .setup = [](prebyte::RenderSession& session) {
            session.variables.set("primary", "false");
            session.variables.set("secondary", "true");
        },
        .expected = "B",
    });
    cases.push_back({
        .label = "for-list",
        .source = "{{ for item in items }}{{ item }};{{ endfor }}",
        .current_file = {},
        .setup = [](prebyte::RenderSession& session) {
            session.variables.set_value("items", prebyte::Value::list(prebyte::Data::Array{prebyte::Data("Ada"), prebyte::Data("Grace")}));
        },
        .expected = "Ada;Grace;",
    });
    cases.push_back({
        .label = "for-list-else",
        .source = "{{ for item in items }}{{ item }}{{ else }}empty{{ endfor }}",
        .current_file = {},
        .setup = [](prebyte::RenderSession& session) {
            session.variables.set_value("items", prebyte::Value::list(prebyte::Data::Array{}));
        },
        .expected = "empty",
    });
    cases.push_back({
        .label = "for-object",
        .source = "{{ for key, value in user }}{{ key }}={{ value }};{{ endfor }}",
        .current_file = {},
        .setup = [](prebyte::RenderSession& session) {
            session.variables.set_value("user", prebyte::Value::object(prebyte::Data::Map{{"b", prebyte::Data("B")}, {"a", prebyte::Data("A")}}));
        },
        .expected = "a=A;b=B;",
    });
    cases.push_back({
        .label = "nested-if-in-for",
        .source = "{{ for user in users }}{{ if user.admin }}A{{ elseif user.name == \"Grace\" }}G{{ else }}U{{ endif }}{{ endfor }}",
        .current_file = {},
        .setup = [](prebyte::RenderSession& session) {
            session.variables.set_value("users", prebyte::Value::list(prebyte::Data::Array{
                prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Ada")}, {"admin", prebyte::Data(true)}}),
                prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Grace")}, {"admin", prebyte::Data(false)}}),
                prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Linus")}, {"admin", prebyte::Data(false)}}),
            }));
        },
        .expected = "AGU",
    });
    cases.push_back({
        .label = "nested-for-in-if",
        .source = "{{ if groups }}{{ for group in groups }}{{ if group.featured }}*{{ else }}-{{ endif }}{{ for member in group.members }}{{ member }}{{ endfor }};{{ endfor }}{{ elseif archived }}archived{{ else }}empty{{ endif }}",
        .current_file = {},
        .setup = [](prebyte::RenderSession& session) {
            session.variables.set("archived", "false");
            session.variables.set_value("groups", prebyte::Value::list(prebyte::Data::Array{
                prebyte::Data(prebyte::Data::Map{{"featured", prebyte::Data(true)}, {"members", prebyte::Data(prebyte::Data::Array{prebyte::Data("A"), prebyte::Data("B")})}}),
                prebyte::Data(prebyte::Data::Map{{"featured", prebyte::Data(false)}, {"members", prebyte::Data(prebyte::Data::Array{prebyte::Data("C")})}}),
            }));
        },
        .expected = "*AB;-C;",
    });
    cases.push_back({
        .label = "include-in-if",
        .source = "{{ if enabled }}{{ include \"./if_partial\" }}{{ else }}disabled{{ endif }}",
        .current_file = root / "include-if-main.pbt",
        .setup = [](prebyte::RenderSession& session) {
            session.variables.set("enabled", "true");
            session.variables.set("name", "Ada");
        },
        .expected = "Hello Ada",
    });
    cases.push_back({
        .label = "include-in-for",
        .source = "{{ for item in items }}{{ include \"./for_partial\" }}{{ endfor }}",
        .current_file = root / "include-for-main.pbt",
        .setup = [](prebyte::RenderSession& session) {
            session.variables.set_value("items", prebyte::Value::list(prebyte::Data::Array{prebyte::Data("Ada"), prebyte::Data("Grace")}));
        },
        .expected = "<1:Ada><2:Grace>",
    });
    cases.push_back({
        .label = "loop-metadata",
        .source = "{{ for item in items }}{{ loop.index }}/{{ loop.last }};{{ endfor }}",
        .current_file = {},
        .setup = [](prebyte::RenderSession& session) {
            session.variables.set_value("items", prebyte::Value::list(prebyte::Data::Array{prebyte::Data("Ada"), prebyte::Data("Grace")}));
        },
        .expected = "1/false;2/true;",
    });
    cases.push_back({
        .label = "lua-condition",
        .source = "{{ if lua(\"return starts_with(name, 'Ada')\") }}ok{{ else }}bad{{ endif }}",
        .current_file = {},
        .setup = [](prebyte::RenderSession& session) { session.variables.set("name", "Ada Lovelace"); },
        .expected = "ok",
    });

    for (const auto& control_case : cases) {
        if (!control_case.current_file.empty()) {
            write_test_file(control_case.current_file, control_case.source);
        }

        prebyte::RenderSession source_session;
        control_case.setup(source_session);
        const std::string source_output = renderer.render_source(control_case.source, settings, control_case.current_file, source_session);

        const prebyte::CompiledProgram program = compiler.compile_source(
            control_case.source,
            control_case.current_file,
            control_case.current_file,
            settings);
        const prebyte::CompiledProgram loaded = serializer.deserialize(serializer.serialize(program));

        prebyte::RenderSession compiled_session;
        control_case.setup(compiled_session);
        const std::string compiled_output = renderer.render_program(loaded, settings, loaded.logical_path, compiled_session);

        require_case_output(control_case.label + " source", source_output, control_case.expected);
        require_case_output(control_case.label + " compiled", compiled_output, control_case.expected);
        require_case_output(control_case.label + " parity", compiled_output, source_output);
    }
}

TEST_CASE(Renderer_render_filters_set_whitespace_and_comparisons) {
    const std::filesystem::path root = renderer_test_root("filters-set-whitespace");
    write_test_file(root / "partial.pbt", "{{ title }}");

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    prebyte::CompiledTemplateSerializer serializer;

    const std::string source =
        "{{ set title = name | trim | upper }}"
        "{{ if title == \"ADA\" && price >= min_price && sku in allowed }}"
        "X{{- include \"./partial\" -}}Y"
        "{{ else }}bad{{ endif }}"
        " {{ missing | default(\"done\") }}";

    auto make_session = []() {
        prebyte::RenderSession session;
        session.variables.set("name", "  Ada  ");
        session.variables.set("price", "10");
        session.variables.set("min_price", "9");
        session.variables.set("sku", "abc");
        session.variables.set_value("allowed", prebyte::Value::list(prebyte::Data::Array{prebyte::Data("abc"), prebyte::Data("xyz")}));
        return session;
    };

    prebyte::RenderSession source_session = make_session();
    const std::string source_output = renderer.render_source(source, settings, root / "main.pbt", source_session);

    const prebyte::CompiledProgram program = compiler.compile_source(source, root / "main.pbt", root / "main.pbt", settings);
    const prebyte::CompiledProgram loaded = serializer.deserialize(serializer.serialize(program));
    prebyte::RenderSession compiled_session = make_session();
    const std::string compiled_output = renderer.render_program(loaded, settings, loaded.logical_path, compiled_session);

    REQUIRE_EQ(source_output, std::string("XADAY done"));
    REQUIRE_EQ(compiled_output, source_output);
}

TEST_CASE(Renderer_set_is_scoped_to_branch_and_include) {
    const std::filesystem::path root = renderer_test_root("set-scope-include");
    write_test_file(root / "partial.pbt", "[{{ label }}]");

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");

    const std::string output = renderer.render_source(
        "{{ set label = \"outer\" }}{{ if name }}{{ set label = name | upper }}{{ include \"./partial\" }}{{ endif }}{{ label }}",
        settings, root / "main.pbt", session);

    REQUIRE_EQ(output, std::string("[ADA]outer"));
}

TEST_CASE(Renderer_set_is_fresh_per_loop_iteration) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set_value("items", prebyte::Value::list(prebyte::Data::Array{prebyte::Data("Ada"), prebyte::Data("Grace")}));

    const std::string output = renderer.render_source(
        "{{ for item in items }}{{ set x = item | upper }}{{ x }};{{ endfor }}{{ if x }}bad{{ else }}ok{{ endif }}",
        settings, "inline", session);

    REQUIRE_EQ(output, std::string("ADA;GRACE;ok"));
}

TEST_CASE(Renderer_replace_flattened_dotted_variable_compat) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("user.name", "Ada");

    const std::string output = renderer.render_source("Hello {{ user.name }}", settings, "inline", session);
    REQUIRE_EQ(output, std::string("Hello Ada"));
}

TEST_CASE(Renderer_replace_object_member) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");
    session.variables.set_value("user", prebyte::Value::object(std::move(user)));

    const std::string output = renderer.render_source("Hello {{ user.name }}", settings, "inline", session);
    REQUIRE_EQ(output, std::string("Hello Ada"));
}

TEST_CASE(Renderer_fail_when_rendering_object_directly) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");
    session.variables.set_value("user", prebyte::Value::object(std::move(user)));

    REQUIRE_THROWS_AS(renderer.render_source("{{ user }}", settings, "inline", session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_render_source_to_keeps_partial_output_before_error) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");
    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");
    session.variables.set_value("user", prebyte::Value::object(std::move(user)));

    std::string output;
    bool threw = false;
    try {
        renderer.render_source_to("Hello {{ name }} {{ user }}", settings, "inline", session,
                                  [&](std::string_view chunk) { output.append(chunk.data(), chunk.size()); });
    } catch (const prebyte::DiagnosticError&) {
        threw = true;
    }

    REQUIRE(threw);
    REQUIRE_EQ(output, std::string("Hello Ada "));
}

TEST_CASE(Renderer_render_serialized_compiled_program_with_object_member) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    const prebyte::CompiledProgram program = compiler.compile_source("Hello {{ user.name }}", "inline", "inline", settings);
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram loaded = serializer.deserialize(serializer.serialize(program));

    prebyte::RenderSession session;
    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");
    session.variables.set_value("user", prebyte::Value::object(std::move(user)));

    const std::string output = renderer.render_program(loaded, settings, loaded.logical_path, session);
    REQUIRE_EQ(output, std::string("Hello Ada"));
}

TEST_CASE(Renderer_render_program_to_matches_collected_output) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    const prebyte::CompiledProgram program = compiler.compile_source(
        "{{ for item in items }}{{ loop.index0 }}={{ item }};{{ else }}empty{{ endfor }}", "inline", "inline", settings);
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram loaded = serializer.deserialize(serializer.serialize(program));

    auto make_session = []() {
        prebyte::RenderSession session;
        prebyte::Data::Array items;
        items.push_back(prebyte::Data("Ada"));
        items.push_back(prebyte::Data("Grace"));
        session.variables.set_value("items", prebyte::Value::list(std::move(items)));
        return session;
    };

    prebyte::RenderSession streamed_session = make_session();
    std::string streamed;
    std::size_t chunk_count = 0;
    renderer.render_program_to(loaded, settings, loaded.logical_path, streamed_session, [&](std::string_view chunk) {
        ++chunk_count;
        streamed.append(chunk.data(), chunk.size());
    });

    prebyte::RenderSession collected_session = make_session();
    const std::string collected = renderer.render_program(loaded, settings, loaded.logical_path, collected_session);

    REQUIRE_EQ(streamed, collected);
    REQUIRE(chunk_count > 1);
}

TEST_CASE(Renderer_render_serialized_program_with_nested_control_flow_and_include) {
    const std::filesystem::path root = renderer_test_root("serialized-nested-control-flow");
    write_test_file(root / "partial.pbt", "{{ if member.admin }}*{{ else }}-{{ endif }}{{ member.name }}");

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    const std::string source =
        "{{ if members }}{{ for member in members }}{{ include \"./partial\" }};{{ endfor }}{{ elseif archived }}archived{{ else }}empty{{ endif }}";
    const prebyte::CompiledProgram program = compiler.compile_source(source, root / "main.pbt", root / "main.pbt", settings);
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram loaded = serializer.deserialize(serializer.serialize(program));

    prebyte::RenderSession session;
    session.variables.set("archived", "false");
    prebyte::Data::Array members;
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Ada")}, {"admin", prebyte::Data(true)}}));
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Grace")}, {"admin", prebyte::Data(false)}}));
    session.variables.set_value("members", prebyte::Value::list(std::move(members)));

    const std::string output = renderer.render_program(loaded, settings, loaded.logical_path, session);
    REQUIRE_EQ(output, std::string("*Ada;-Grace;"));
}

TEST_CASE(Renderer_replace_list_index) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    const std::string output = renderer.render_source("Hello {{ items[0] }}", settings, "inline", session);
    REQUIRE_EQ(output, std::string("Hello Ada"));
}

TEST_CASE(Renderer_render_serialized_compiled_program_with_list_index) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    const prebyte::CompiledProgram program = compiler.compile_source("Hello {{ items[1] }}", "inline", "inline", settings);
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram loaded = serializer.deserialize(serializer.serialize(program));

    prebyte::RenderSession session;
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    const std::string output = renderer.render_program(loaded, settings, loaded.logical_path, session);
    REQUIRE_EQ(output, std::string("Hello Grace"));
}

TEST_CASE(Renderer_fail_on_invalid_args_postfix_member) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.args = {"first"};

    REQUIRE_THROWS_AS(renderer.render_source("{{ ARGS.name }}", settings, "inline", session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_render_len_for_string_list_object_and_scalars) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");

    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");
    session.variables.set_value("user", prebyte::Value::object(std::move(user)));
    session.variables.set_value("enabled", prebyte::Value(true));
    session.variables.set_value("count", prebyte::Value(42.0));

    const std::string output = renderer.render_source(
        "{{ len(name) }}/{{ len(items) }}/{{ len(user) }}/{{ len(missing) }}/{{ len(enabled) }}/{{ len(count) }}",
        settings, "inline", session);
    REQUIRE_EQ(output, std::string("3/2/1/0/0/0"));
}

TEST_CASE(Renderer_render_serialized_compiled_program_with_len) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    const prebyte::CompiledProgram program = compiler.compile_source(
        "{{ len(items) }}/{{ len(user.name) }}/{{ len(missing) }}", "inline", "inline", settings);
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram loaded = serializer.deserialize(serializer.serialize(program));

    prebyte::RenderSession session;
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");
    session.variables.set_value("user", prebyte::Value::object(std::move(user)));

    const std::string output = renderer.render_program(loaded, settings, loaded.logical_path, session);
    REQUIRE_EQ(output, std::string("2/3/0"));
}

TEST_CASE(Renderer_render_for_loop_over_list) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("item", "outer");

    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    const std::string output = renderer.render_source("{{ for item in items }}{{ item }} {{ endfor }}{{ item }}",
                                                      settings, "inline", session);
    REQUIRE_EQ(output, std::string("Ada Grace outer"));
}

TEST_CASE(Renderer_render_for_else_when_list_empty) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set_value("items", prebyte::Value::list({}));

    const std::string output = renderer.render_source("{{ for item in items }}{{ item }}{{ else }}empty{{ endfor }}",
                                                      settings, "inline", session);
    REQUIRE_EQ(output, std::string("empty"));
}

TEST_CASE(Renderer_render_for_else_when_iterable_null) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    const std::string output = renderer.render_source("{{ for item in missing }}{{ item }}{{ else }}empty{{ endfor }}",
                                                      settings, "inline", session);
    REQUIRE_EQ(output, std::string("empty"));
}

TEST_CASE(Renderer_strict_missing_iterable_in_for_loop_fails) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.strict_variables = true;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        renderer.render_source("{{ for item in missing }}{{ item }}{{ else }}empty{{ endfor }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_strict_explicit_null_iterable_in_for_loop_uses_else) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.strict_variables = true;
    prebyte::RenderSession session;
    session.variables.set_value("items", prebyte::Value());

    const std::string output = renderer.render_source("{{ for item in items }}{{ item }}{{ else }}empty{{ endfor }}",
                                                      settings, "inline", session);
    REQUIRE_EQ(output, std::string("empty"));
}

TEST_CASE(Renderer_strict_missing_nested_member_in_loop_condition_fails) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.strict_variables = true;
    prebyte::RenderSession session;

    prebyte::Data::Array users;
    users.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Ada")}}));
    session.variables.set_value("users", prebyte::Value::list(std::move(users)));

    REQUIRE_THROWS_AS(
        renderer.render_source("{{ for user in users }}{{ if user.nickname }}Y{{ else }}N{{ endif }}{{ endfor }}",
                               settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_default_variable_value_applies_in_nested_loop_condition) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.default_variable_value = "Fallback";
    prebyte::RenderSession session;

    prebyte::Data::Array users;
    users.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Ada")}}));
    users.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Grace")}}));
    session.variables.set_value("users", prebyte::Value::list(std::move(users)));

    const std::string output = renderer.render_source(
        "{{ for user in users }}{{ if user.nickname }}Y{{ else }}N{{ endif }}{{ endfor }}",
        settings, "inline", session);
    REQUIRE_EQ(output, std::string("YY"));
}

TEST_CASE(Renderer_render_for_loop_over_object_in_key_order) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    prebyte::Data::Map user;
    user["b"] = prebyte::Data("B");
    user["a"] = prebyte::Data("A");
    session.variables.set_value("user", prebyte::Value::object(std::move(user)));

    const std::string output = renderer.render_source("{{ for key, value in user }}{{ key }}={{ value }};{{ endfor }}",
                                                      settings, "inline", session);
    REQUIRE_EQ(output, std::string("a=A;b=B;"));
}

TEST_CASE(Renderer_for_loop_values_visible_in_include) {
    const std::filesystem::path root = renderer_test_root("loop-include-scope");
    write_test_file(root / "partial.pbt", "<{{ item }}>");

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    const std::string output = renderer.render_source("{{ for item in items }}{{ include \"./partial\" }}{{ endfor }}",
                                                      settings, root / "main.pbt", session);
    REQUIRE_EQ(output, std::string("<Ada><Grace>"));
}

TEST_CASE(Renderer_render_serialized_compiled_program_with_for_else) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    const prebyte::CompiledProgram program = compiler.compile_source(
        "{{ for item in items }}{{ item }}{{ else }}empty{{ endfor }}", "inline", "inline", settings);
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram loaded = serializer.deserialize(serializer.serialize(program));

    prebyte::RenderSession session;
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    const std::string output = renderer.render_program(loaded, settings, loaded.logical_path, session);
    REQUIRE_EQ(output, std::string("AdaGrace"));
}

TEST_CASE(Renderer_render_loop_metadata_fields) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    const std::string output = renderer.render_source(
        "{{ for item in items }}{{ loop.index }}/{{ loop.index0 }}/{{ loop.first }}/{{ loop.last }};{{ endfor }}",
        settings, "inline", session);
    REQUIRE_EQ(output, std::string("1/0/true/false;2/1/false/true;"));
}

TEST_CASE(Renderer_loop_metadata_visible_in_include) {
    const std::filesystem::path root = renderer_test_root("loop-meta-include");
    write_test_file(root / "partial.pbt", "{{ loop.index }}:{{ loop.last }}");

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    const std::string output = renderer.render_source("{{ for item in items }}{{ include \"./partial\" }};{{ endfor }}",
                                                      settings, root / "main.pbt", session);
    REQUIRE_EQ(output, std::string("1:false;2:true;"));
}

TEST_CASE(Renderer_loop_metadata_shadows_outer_loop_when_nested) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    prebyte::Data::Array inner;
    inner.push_back(prebyte::Data("x"));
    inner.push_back(prebyte::Data("y"));

    prebyte::Data::Array outers;
    outers.push_back(prebyte::Data(inner));
    outers.push_back(prebyte::Data(inner));
    session.variables.set_value("outers", prebyte::Value::list(std::move(outers)));

    const std::string output = renderer.render_source(
        "{{ for outer in outers }}O{{ loop.index }}[{{ for item in outer }}{{ loop.index0 }}{{ endfor }}]{{ endfor }}",
        settings, "inline", session);
    REQUIRE_EQ(output, std::string("O1[01]O2[01]"));
}

TEST_CASE(Renderer_loop_outside_body_uses_normal_missing_semantics) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    const std::string output = renderer.render_source("{{ loop.index }}", settings, "inline", session);
    REQUIRE_EQ(output, std::string(""));
}

TEST_CASE(Renderer_render_serialized_compiled_program_with_loop_metadata) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    const prebyte::CompiledProgram program = compiler.compile_source(
        "{{ for item in items }}{{ loop.index0 }}:{{ loop.last }};{{ endfor }}", "inline", "inline", settings);
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram loaded = serializer.deserialize(serializer.serialize(program));

    prebyte::RenderSession session;
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    items.push_back(prebyte::Data("Grace"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    const std::string output = renderer.render_program(loaded, settings, loaded.logical_path, session);
    REQUIRE_EQ(output, std::string("0:false;1:true;"));
}

TEST_CASE(Renderer_render_plain_elseif_branch) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("primary", "false");
    session.variables.set("secondary", "true");

    const std::string output = renderer.render_source(
        "{{ if primary }}bad{{ elseif secondary }}ok{{ else }}fallback{{ endif }}",
        settings, "inline", session);
    REQUIRE_EQ(output, std::string("ok"));
}

TEST_CASE(Renderer_render_nested_if_inside_for_with_elseif) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    prebyte::Data::Array users;
    users.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Ada")}, {"admin", prebyte::Data(true)}}));
    users.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Grace")}, {"admin", prebyte::Data(false)}}));
    users.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Linus")}, {"admin", prebyte::Data(false)}}));
    session.variables.set_value("users", prebyte::Value::list(std::move(users)));

    const std::string output = renderer.render_source(
        "{{ for user in users }}{{ if user.admin }}A{{ elseif user.name == \"Grace\" }}G{{ else }}U{{ endif }}{{ endfor }}",
        settings, "inline", session);
    REQUIRE_EQ(output, std::string("AGU"));
}

TEST_CASE(Renderer_render_nested_for_inside_if_with_elseif) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("archived", "false");

    prebyte::Data::Array groups;
    groups.push_back(prebyte::Data(prebyte::Data::Map{
        {"featured", prebyte::Data(true)},
        {"members", prebyte::Data(prebyte::Data::Array{prebyte::Data("A"), prebyte::Data("B")})},
    }));
    groups.push_back(prebyte::Data(prebyte::Data::Map{
        {"featured", prebyte::Data(false)},
        {"members", prebyte::Data(prebyte::Data::Array{prebyte::Data("C")})},
    }));
    session.variables.set_value("groups", prebyte::Value::list(std::move(groups)));

    const std::string output = renderer.render_source(
        "{{ if groups }}{{ for group in groups }}{{ if group.featured }}*{{ else }}-{{ endif }}{{ for member in group.members }}{{ member }}{{ endfor }};{{ endfor }}{{ elseif archived }}archived{{ else }}empty{{ endif }}",
        settings, "inline", session);
    REQUIRE_EQ(output, std::string("*AB;-C;"));
}

TEST_CASE(Renderer_fail_when_include_cycle_happens_inside_if_branch) {
    const std::filesystem::path root = renderer_test_root("if-include-cycle");
    write_test_file(root / "main.pbt", "{{ if enabled }}{{ include \"./partial\" }}{{ endif }}");
    write_test_file(root / "partial.pbt", "{{ if enabled }}{{ include \"./main\" }}{{ endif }}");

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("enabled", "true");

    REQUIRE_THROWS_AS(renderer.render_source("{{ if enabled }}{{ include \"./partial\" }}{{ endif }}",
                                             settings, root / "main.pbt", session),
                      prebyte::DiagnosticError);
}

TEST_CASE(Renderer_fail_when_comparing_structured_values_in_if_condition) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    prebyte::Data::Map user;
    user["name"] = prebyte::Data("Ada");
    session.variables.set_value("user", prebyte::Value::object(std::move(user)));

    REQUIRE_THROWS_AS(
        renderer.render_source("{{ if user == user }}bad{{ else }}ok{{ endif }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_fail_when_comparing_structured_values_in_serialized_program_condition) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::CompiledTemplateCompiler compiler;
    const prebyte::CompiledProgram program = compiler.compile_source(
        "{{ if items == items }}bad{{ else }}ok{{ endif }}", "inline", "inline", settings);
    prebyte::CompiledTemplateSerializer serializer;
    const prebyte::CompiledProgram loaded = serializer.deserialize(serializer.serialize(program));

    prebyte::RenderSession session;
    prebyte::Data::Array items;
    items.push_back(prebyte::Data("Ada"));
    session.variables.set_value("items", prebyte::Value::list(std::move(items)));

    REQUIRE_THROWS_AS(renderer.render_program(loaded, settings, loaded.logical_path, session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_strict_missing_variable_fails) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.strict_variables = true;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(renderer.render_source("Hello {{ missing }}", settings, "inline", session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_execute_lua_expression_and_keep_lazy_runtime) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");

    REQUIRE(!session.lua_runtime);
    REQUIRE_EQ(renderer.render_source("{{ lua \"return upper(name)\" }}", settings, "inline", session), std::string("ADA"));
    REQUIRE(session.lua_runtime != nullptr);
}

TEST_CASE(Renderer_execute_registered_lua_lower_helper) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");

    REQUIRE_EQ(renderer.render_source("{{ lua \"return lower(upper(name))\" }}", settings, "inline", session), std::string("ada"));
}

TEST_CASE(Renderer_execute_registered_lua_text_helpers) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada Lovelace");
    session.variables.set("padded", "  Ada Lovelace  ");

    REQUIRE_EQ(renderer.render_source("{{ lua \"return trim(padded)\" }}", settings, "inline", session), std::string("Ada Lovelace"));
    REQUIRE_EQ(
        renderer.render_source("{{ if lua(\"return starts_with(name, 'Ada') and ends_with(name, 'lace')\") }}ok{{ else }}bad{{ endif }}",
                               settings, "inline", session),
        std::string("ok"));
}

TEST_CASE(Renderer_fail_when_lua_helper_called_with_bad_arity) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        renderer.render_source("{{ lua \"return starts_with('Ada')\" }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_native_path_without_lua_keeps_runtime_null) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");

    REQUIRE_EQ(renderer.render_source("Hello {{ name }}", settings, "inline", session), std::string("Hello Ada"));
    REQUIRE(!session.lua_runtime);
}

TEST_CASE(Renderer_isolate_lua_globals_between_cached_executions) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    const std::string source =
        "{{ lua \"counter = (counter or 0) + 1; return counter\" }} "
        "{{ lua \"counter = (counter or 0) + 1; return counter\" }}";

    REQUIRE_EQ(renderer.render_source(source, settings, "inline", session), std::string("1 1"));
}

TEST_CASE(Renderer_fail_when_lua_exceeds_memory_limit) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        renderer.render_source("{{ lua \"return string.rep('x', 8388608)\" }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_fail_when_lua_exceeds_configured_memory_limit) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.lua_memory_limit_bytes = 1024 * 1024;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        renderer.render_source("{{ lua \"return string.rep('x', 2097152)\" }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_fail_when_lua_exceeds_configured_instruction_limit) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.lua_instruction_limit = 10;
    prebyte::RenderSession session;

    REQUIRE_THROWS_AS(
        renderer.render_source("{{ lua \"local sum = 0 for i = 1, 1000 do sum = sum + i end return sum\" }}", settings, "inline", session),
        prebyte::DiagnosticError);
}

TEST_CASE(Renderer_replace_args_index) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.args = {"first", "second"};

    const std::string output = renderer.render_source("{{ ARGS[1] }}", settings, "inline", session);
    REQUIRE_EQ(output, std::string("second"));
}

TEST_CASE(Renderer_fail_on_bare_args_identifier) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.args = {"first"};

    REQUIRE_THROWS_AS(renderer.render_source("{{ ARGS }}", settings, "inline", session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_expose_args_to_lua) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    session.args = {"zero", "one"};

    REQUIRE_EQ(renderer.render_source("{{ lua \"return ARGS[0]\" }}", settings, "inline", session), std::string("zero"));
}

TEST_CASE(Renderer_render_template_function_after_definition) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    const std::string output = renderer.render_source(
        "{{ fn greet(name) }}Hello {{ name }}{{ endfn }}{{ greet(\"Ada\") }}",
        settings,
        "inline",
        session);

    REQUIRE_EQ(output, std::string("Hello Ada"));
}

TEST_CASE(Renderer_render_lua_function_with_structured_return) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    const std::string output = renderer.render_source(
        "{{ fn users() lua:block }}return { { name = \"Ada\" }, { name = \"Grace\" } }{{ endfn }}{{ for user in users() }}{{ user.name }};{{ endfor }}",
        settings,
        "inline",
        session);

    REQUIRE_EQ(output, std::string("Ada;Grace;"));
}

TEST_CASE(Renderer_function_definition_visible_in_later_include_only) {
    const std::filesystem::path root = renderer_test_root("function-include-scope");
    write_test_file(root / "later.pbt", "{{ greet(\"Ada\") }}");
    write_test_file(root / "definer.pbt", "{{ fn greet(name) }}Hello {{ name }}{{ endfn }}{{ include \"./later\" }}");

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    REQUIRE_EQ(renderer.render_source("{{ include \"./definer\" }}", settings, root / "main.pbt", session), std::string("Hello Ada"));
    REQUIRE_THROWS_AS(renderer.render_source("{{ greet(\"Ada\") }}", settings, root / "main.pbt", session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_function_call_before_definition_fails) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    REQUIRE_THROWS_AS(renderer.render_source("{{ greet(\"Ada\") }}{{ fn greet(name) }}Hi{{ endfn }}", settings, "inline", session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_dynamic_builtins_are_constant_per_render) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    const std::string output = renderer.render_source(
        "{{ __UUID__ == __UUID__ }}|{{ __RANDOM__ == __RANDOM__ }}|{{ __DATE__ }}|{{ __TIMESTAMP__ }}|{{ __YEAR__ }}-{{ __MONTH__ }}-{{ __DAY__ }}",
        settings,
        "inline",
        session);

    REQUIRE(output.find("true|true|") == 0);
}

TEST_CASE(Renderer_file_and_system_builtins_render) {
    const std::filesystem::path root = renderer_test_root("builtin-file-values");
    const std::filesystem::path current_file = root / "sample.pbt";

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    prebyte::RenderSession session;
    const std::string output = renderer.render_source(
        "{{ __FILENAME__ }}|{{ __EXTENSION__ }}|{{ __DIR__ }}|{{ __OS__ == __OS__ }}|{{ __WORKING_DIR__ == __WORKING_DIR__ }}",
        settings,
        current_file,
        session);

    REQUIRE(output.find("sample|pbt|") == 0);
    REQUIRE(output.find("|true|true") != std::string::npos);
}

TEST_CASE(Renderer_case_sensitive_variables_rule_changes_lookup_behavior) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.case_sensitive_variables = false;

    prebyte::RenderSession session;
    session.variables.set("Name", "Ada");
    REQUIRE_EQ(renderer.render_source("{{ name }}", settings, "inline", session), std::string("Ada"));

    settings.case_sensitive_variables = true;
    REQUIRE_EQ(renderer.render_source("{{ name }}", settings, "inline", session), std::string());
}

TEST_CASE(Renderer_max_variable_length_rule_truncates_values) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.has_max_variable_length = true;
    settings.max_variable_length = 3;

    prebyte::RenderSession session;
    session.variables.set("name", "Ada Lovelace");
    REQUIRE_EQ(renderer.render_source("{{ name }}", settings, "inline", session), std::string("Ada"));
}

TEST_CASE(Renderer_allow_env_and_forbidden_env_vars_interaction) {
    ::setenv("PREBYTE_ALLOWED_ENV", "Ada", 1);
    ::setenv("PREBYTE_BLOCKED_ENV", "Secret", 1);

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.allow_env = true;

    prebyte::RenderSession session;
    REQUIRE_EQ(renderer.render_source("{{ PREBYTE_ALLOWED_ENV }}", settings, "inline", session), std::string("Ada"));

    settings.forbidden_env_vars.insert("PREBYTE_BLOCKED_ENV");
    REQUIRE_THROWS_AS(renderer.render_source("{{ PREBYTE_BLOCKED_ENV }}", settings, "inline", session), prebyte::DiagnosticError);

    settings.allow_env = false;
    settings.strict_variables = false;
    settings.default_variable_value = "Fallback";
    REQUIRE_EQ(renderer.render_source("{{ PREBYTE_BLOCKED_ENV }}", settings, "inline", session), std::string("Fallback"));
}

TEST_CASE(Renderer_max_include_depth_rule_enforced_per_include_nesting) {
    const std::filesystem::path root = renderer_test_root("max-include-depth");
    write_test_file(root / "level2.pbt", "deep");
    write_test_file(root / "level1.pbt", "{{ include \"./level2\" }}");
    write_test_file(root / "main.pbt", "{{ include \"./level1\" }}");

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.max_include_depth = 0;
    prebyte::RenderSession session;
    REQUIRE_THROWS_AS(renderer.render_source("{{ include \"./level1\" }}", settings, root / "main.pbt", session), prebyte::DiagnosticError);

    settings.max_include_depth = 2;
    prebyte::RenderSession ok_session;
    REQUIRE_EQ(renderer.render_source("{{ include \"./level1\" }}", settings, root / "main.pbt", ok_session), std::string("deep"));
}

TEST_CASE(Renderer_allow_includes_false_wins_before_include_depth_rule) {
    const std::filesystem::path root = renderer_test_root("allow-includes-before-depth");
    write_test_file(root / "partial.pbt", "x");

    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.allow_includes = false;
    settings.max_include_depth = 0;

    prebyte::RenderSession session;
    REQUIRE_THROWS_AS(renderer.render_source("{{ include \"./partial\" }}", settings, root / "main.pbt", session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_max_output_size_bytes_rule_limits_plain_text) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.max_output_size_bytes = 5;
    prebyte::RenderSession session;
    REQUIRE_EQ(renderer.render_source("Hello", settings, "inline", session), std::string("Hello"));

    prebyte::RenderSession fail_session;
    REQUIRE_THROWS_AS(renderer.render_source("Hello!", settings, "inline", fail_session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_max_output_size_bytes_rule_limits_loop_and_function_output) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.max_output_size_bytes = 4;

    const std::filesystem::path root = renderer_test_root("output-limit-include-lua");
    write_test_file(root / "partial.pbt", "Hello");

    prebyte::RenderSession function_session;
    REQUIRE_THROWS_AS(renderer.render_source("{{ fn greet() }}Hello{{ endfn }}{{ greet() }}", settings, "inline", function_session), prebyte::DiagnosticError);

    prebyte::RenderSession include_session;
    REQUIRE_THROWS_AS(renderer.render_source("{{ include \"./partial\" }}", settings, root / "main.pbt", include_session), prebyte::DiagnosticError);

    prebyte::RenderSession lua_session;
    REQUIRE_THROWS_AS(renderer.render_source("{{ lua \"return 'Hello'\" }}", settings, "inline", lua_session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_max_loop_iteration_rule_enforced_per_loop_instance) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.max_loop_iteration = 2;

    prebyte::RenderSession session;
    session.variables.set_value("items", prebyte::Value::list(prebyte::Data::Array{prebyte::Data("A"), prebyte::Data("B"), prebyte::Data("C")}));
    REQUIRE_THROWS_AS(renderer.render_source("{{ for item in items }}{{ item }}{{ endfor }}", settings, "inline", session), prebyte::DiagnosticError);

    prebyte::RenderSession nested_ok;
    nested_ok.variables.set_value(
        "groups",
        prebyte::Value::list(prebyte::Data::Array{
            prebyte::Data(prebyte::Data::Array{prebyte::Data("A"), prebyte::Data("B")}),
            prebyte::Data(prebyte::Data::Array{prebyte::Data("C"), prebyte::Data("D")}),
        }));
    REQUIRE_EQ(renderer.render_source("{{ for group in groups }}{{ for item in group }}{{ item }}{{ endfor }}|{{ endfor }}", settings, "inline", nested_ok), std::string("AB|CD|"));
}

TEST_CASE(Renderer_max_loop_iteration_rule_also_applies_to_object_loops) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.max_loop_iteration = 1;

    prebyte::RenderSession session;
    session.variables.set_value("mapping", prebyte::Value::object(prebyte::Data::Map{{"a", prebyte::Data("A")}, {"b", prebyte::Data("B")}}));
    REQUIRE_THROWS_AS(renderer.render_source("{{ for key, value in mapping }}{{ key }}={{ value }}{{ endfor }}", settings, "inline", session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_max_render_time_rule_interrupts_lua) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.max_render_time_ms = 0;

    prebyte::RenderSession session;
    REQUIRE_THROWS_AS(renderer.render_source("{{ lua:block }} while true do end return 'x' {{ endlua }}", settings, "inline", session), prebyte::DiagnosticError);
}

TEST_CASE(Renderer_error_on_false_input_rejects_false_if_and_elseif_conditions) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator(builtins);
    prebyte::Renderer renderer(rule_resolver, include_resolver, evaluator);

    prebyte::EffectiveSettings settings;
    settings.error_on_false_input = true;

    prebyte::RenderSession if_session;
    try {
        static_cast<void>(renderer.render_source("{{ if false }}bad{{ else }}ok{{ endif }}", settings, "inline", if_session));
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError& error) {
        REQUIRE_EQ(error.diagnostic().message, std::string("False input is not allowed in condition"));
    }

    prebyte::RenderSession elseif_session;
    elseif_session.variables.set("primary", "false");
    elseif_session.variables.set("secondary", "false");
    try {
        static_cast<void>(renderer.render_source("{{ if primary }}A{{ elseif secondary }}B{{ else }}C{{ endif }}", settings, "inline", elseif_session));
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError& error) {
        REQUIRE_EQ(error.diagnostic().message, std::string("False input is not allowed in condition"));
    }

    settings.error_on_false_input = false;
    prebyte::RenderSession relaxed_session;
    REQUIRE_EQ(renderer.render_source("{{ if false }}bad{{ else }}ok{{ endif }}", settings, "inline", relaxed_session), std::string("ok"));
}
