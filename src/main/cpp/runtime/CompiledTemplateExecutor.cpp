#include "runtime/CompiledTemplateExecutor.h"

#include "config/RuleResolver.h"
#include "runtime/CompiledTemplateCompiler.h"
#include "runtime/CompiledTemplateCache.h"
#include "runtime/FileMetadataCache.h"
#include "runtime/CompiledTemplateSerializer.h"
#include "runtime/CompiledTemplateWriter.h"
#include "support/Diagnostic.h"
#include "support/TextUtil.h"

#include <bit>
#include <compare>
#include <limits>

namespace prebyte {

namespace {

struct LoopMeta {
    std::size_t else_index = std::numeric_limits<std::size_t>::max();
    std::size_t end_index = std::numeric_limits<std::size_t>::max();
};

LoopMeta find_loop_meta(const CompiledProgram& program, std::size_t for_index) {
    LoopMeta meta;
    std::size_t depth = 0;
    for (std::size_t index = for_index + 1; index < program.template_instructions.size(); ++index) {
        const TemplateOpcode opcode = program.template_instructions[index].opcode;
        if (opcode == TemplateOpcode::ForLoop) {
            ++depth;
            continue;
        }
        if (opcode == TemplateOpcode::EndFor) {
            if (depth == 0) {
                meta.end_index = index;
                return meta;
            }
            --depth;
            continue;
        }
        if (opcode == TemplateOpcode::ElseLoop && depth == 0 && meta.else_index == std::numeric_limits<std::size_t>::max()) {
            meta.else_index = index;
        }
    }
    return meta;
}

struct LoopBindingNames {
    std::string_view first;
    std::string_view second;
    bool has_second = false;
};

LoopBindingNames parse_loop_bindings(std::string_view encoded) {
    const std::size_t separator = encoded.find('\0');
    if (separator == std::string_view::npos) {
        return LoopBindingNames{.first = encoded};
    }
    return LoopBindingNames{
        .first = encoded.substr(0, separator),
        .second = encoded.substr(separator + 1),
        .has_second = true,
    };
}

RenderSession::LoopFrame make_loop_frame(std::string_view first_name, Value first_value,
                                         std::size_t index0, std::size_t size) {
    RenderSession::LoopFrame frame;
    frame.binding_name_0.assign(first_name);
    frame.binding_value_0 = std::move(first_value);
    frame.loop_index0 = index0;
    frame.loop_size = size;
    return frame;
}

RenderSession::LoopFrame make_loop_frame(std::string_view first_name, Value first_value,
                                         std::string_view second_name, Value second_value,
                                         std::size_t index0, std::size_t size) {
    RenderSession::LoopFrame frame = make_loop_frame(first_name, std::move(first_value), index0, size);
    frame.binding_name_1.assign(second_name);
    frame.binding_value_1 = std::move(second_value);
    frame.has_second_binding = true;
    return frame;
}

std::optional<Value> try_evaluate_dotted_fast_path(const Value& base, std::string_view path) {
    Value current = base;
    std::size_t segment_start = 0;
    while (segment_start < path.size()) {
        const std::size_t dot = path.find('.', segment_start);
        const std::string_view segment = dot == std::string_view::npos
            ? path.substr(segment_start)
            : path.substr(segment_start, dot - segment_start);
        if (segment.empty()) {
            return std::nullopt;
        }
        const auto member = current.member(segment);
        if (!member.has_value()) {
            return std::nullopt;
        }
        current = *member;
        if (dot == std::string_view::npos) {
            return current;
        }
        segment_start = dot + 1;
    }
    return current;
}

std::optional<Value> try_fast_loop_lookup(std::string_view name, bool case_sensitive, const RenderSession& session) {
    const std::size_t dot = name.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 >= name.size()) {
        return std::nullopt;
    }

    const std::string_view root = name.substr(0, dot);
    const Value* scoped_value = session.lookup_scoped_value(root, case_sensitive);
    if (scoped_value == nullptr) {
        return std::nullopt;
    }
    return try_evaluate_dotted_fast_path(*scoped_value, name.substr(dot + 1));
}

Diagnostic make_exec_error(const std::string& message, const std::filesystem::path& path) {
    Diagnostic diagnostic;
    diagnostic.code = "EXEC001";
    diagnostic.message = message;
    diagnostic.span.file_path = path.string();
    return diagnostic;
}

void ensure_render_time_budget(const EffectiveSettings& settings, const std::filesystem::path& current_file,
                               const RenderSession& session) {
    if (session.render_time_exceeded(settings)) {
        throw DiagnosticError(make_exec_error("Render time limit exceeded", current_file));
    }
}

void ensure_include_depth_limit(const EffectiveSettings& settings, const std::filesystem::path& current_file,
                                const RenderSession& session) {
    if (settings.max_include_depth != std::numeric_limits<std::size_t>::max()
        && session.include_stack.size() >= settings.max_include_depth) {
        throw DiagnosticError(make_exec_error("Include depth limit exceeded", current_file));
    }
}

void ensure_loop_iteration_limit(const EffectiveSettings& settings, const std::filesystem::path& current_file,
                                 std::size_t iterations) {
    if (settings.max_loop_iteration != std::numeric_limits<std::size_t>::max()
        && iterations > settings.max_loop_iteration) {
        throw DiagnosticError(make_exec_error("Loop iteration limit exceeded", current_file));
    }
}

void emit_chunk(std::string_view chunk, const EffectiveSettings& settings,
                const std::filesystem::path& current_file, RenderSession& session,
                const CompiledTemplateExecutor::ChunkSink& sink, bool count_output) {
    if (chunk.empty()) {
        return;
    }
    if (count_output && session.output_limit_would_exceed(settings, chunk.size())) {
        throw DiagnosticError(make_exec_error("Output size limit exceeded", current_file));
    }
    sink(chunk);
    if (count_output) {
        session.note_output_bytes(chunk.size());
    }
}

Diagnostic make_runtime_error(const std::string& message, const std::filesystem::path& path,
                              std::uint32_t line, std::uint32_t column) {
    Diagnostic diagnostic;
    diagnostic.code = "RUNTIME001";
    diagnostic.message = message;
    diagnostic.span.file_path = path.string();
    diagnostic.span.start.line = line;
    diagnostic.span.start.column = column;
    diagnostic.span.end = diagnostic.span.start;
    return diagnostic;
}

std::pair<std::uint32_t, std::uint32_t> condition_location(const CompiledProgram& program, InstructionRange range) {
    if (range.length == 0 || range.offset >= program.expression_instructions.size()) {
        return {1, 1};
    }

    const ExpressionInstruction& instruction = program.expression_instructions[range.offset];
    return {
        instruction.arg0 == 0 ? 1u : instruction.arg0,
        instruction.arg1 == 0 ? 1u : instruction.arg1,
    };
}

Diagnostic make_include_cycle_error(const std::filesystem::path& path, const RenderSession& session) {
    Diagnostic diagnostic;
    diagnostic.code = "RUNTIME002";
    diagnostic.message = "Include cycle detected";
    diagnostic.span.file_path = path.string();
    for (const auto& include : session.include_stack) {
        diagnostic.include_chain.push_back(include.string());
    }
    return diagnostic;
}

const EffectiveSettings& include_settings_for(const RuleResolver& rule_resolver, const EffectiveSettings& current_settings,
                                              const std::filesystem::path& path, RenderSession& session) {
    if (session.effective_settings_cache_ref != nullptr) {
        auto [it, inserted] = session.effective_settings_cache_ref->try_emplace(path);
        if (inserted) {
            it->second = session.configuration_ref != nullptr
                ? rule_resolver.resolve_for_file(session.configuration_view(), path)
                : current_settings;
        }
        return it->second;
    }

    static thread_local EffectiveSettings uncached_settings;
    uncached_settings = session.configuration_ref != nullptr
        ? rule_resolver.resolve_for_file(session.configuration_view(), path)
        : current_settings;
    return uncached_settings;
}

bool can_fast_path_variable_lookup(const EffectiveSettings& settings, const RenderSession& session) {
    return settings.case_sensitive_variables && !settings.trim && !settings.has_max_variable_length
        && session.loop_frames.empty() && !settings.allow_env && !settings.default_variable_value.has_value() && !settings.strict_variables
        && session.ignore_names_view().empty();
}

bool can_fast_path_arg_lookup(const EffectiveSettings& settings) {
    return !settings.trim && !settings.has_max_variable_length;
}

bool try_execute_prepared_include(const CompiledProgram& parent_program, std::uint32_t instruction,
                                  const EffectiveSettings& settings,
                                  CompiledTemplateExecutor const& executor, RenderSession& session,
                                  const CompiledTemplateExecutor::ChunkSink& sink) {
    if (session.prepared_include_cache_ref == nullptr) {
        return false;
    }
    const RenderSession::PreparedIncludeKey key{&parent_program, instruction};
    auto it = session.prepared_include_cache_ref->find(key);
    if (it == session.prepared_include_cache_ref->end()) {
        return false;
    }
    const RenderSession::PreparedIncludeEntry& entry = it->second;
    if (entry.program == nullptr || entry.settings == nullptr || std::chrono::steady_clock::now() >= entry.valid_until) {
        session.prepared_include_cache_ref->erase(it);
        return false;
    }
    if (session.contains_include(entry.logical_path)) {
        throw DiagnosticError(make_include_cycle_error(entry.logical_path, session));
    }
    ensure_include_depth_limit(settings, entry.logical_path, session);

    session.push_include(entry.logical_path);
    session.push_local_scope();
    try {
        executor.execute(*entry.program, *entry.settings, entry.logical_path, session, sink);
    } catch (...) {
        session.pop_local_scope();
        session.pop_include();
        throw;
    }
    session.pop_local_scope();
    session.pop_include();
    return true;
}

void emit_value(const Value& value, const EffectiveSettings& settings,
                const std::filesystem::path& current_file, RenderSession& session,
                const CompiledTemplateExecutor::ChunkSink& sink, bool count_output) {
    if (const auto string_value = value.try_as_string_view()) {
        emit_chunk(*string_value, settings, current_file, session, sink, count_output);
        return;
    }

    const std::string rendered = value.to_string();
    emit_chunk(rendered, settings, current_file, session, sink, count_output);
}

void ensure_scalar_comparison(const Value& left, const Value& right, const std::filesystem::path& current_file,
                              std::uint32_t line, std::uint32_t column) {
    if (left.is_object() || left.is_list() || right.is_object() || right.is_list()) {
        throw DiagnosticError(make_runtime_error("Cannot compare structured values directly", current_file, line, column));
    }
}

void ensure_scalar_operand(const Value& value, const std::filesystem::path& current_file,
                           std::uint32_t line, std::uint32_t column, std::string_view op_name) {
    if (value.is_object() || value.is_list()) {
        throw DiagnosticError(make_runtime_error("Operator '" + std::string(op_name) + "' requires scalar operands", current_file, line, column));
    }
}

bool contains_value(const Value& right, const Value& left) {
    if (const auto string_value = right.try_as_string_view()) {
        return string_value->find(left.to_string()) != std::string_view::npos;
    }
    if (const Value::Object* object = right.try_as_object()) {
        return object->contains(left.to_string());
    }
    if (const Value::List* list = right.try_as_list()) {
        for (const Data& item : *list) {
            Value current = Value::borrowed_data(item);
            if (!current.is_object() && !current.is_list() && current.equals(left)) {
                return true;
            }
        }
        return false;
    }
    return false;
}

bool compare_with_order(std::strong_ordering ordering, std::string_view op_name) {
    if (op_name == "<") {
        return ordering == std::strong_ordering::less;
    }
    if (op_name == ">") {
        return ordering == std::strong_ordering::greater;
    }
    if (op_name == "<=") {
        return ordering == std::strong_ordering::less || ordering == std::strong_ordering::equal;
    }
    return ordering == std::strong_ordering::greater || ordering == std::strong_ordering::equal;
}

const std::filesystem::path& function_file_for(const RenderSession::FunctionDefinition& function,
                                               const std::filesystem::path& fallback) {
    if (!function.definition_file.empty()) {
        return function.definition_file;
    }
    return fallback;
}

}

CompiledTemplateExecutor::CompiledTemplateExecutor(const RuleResolver& rule_resolver,
                                                   const IncludeResolver& include_resolver,
                                                   const BuiltinRegistry& builtins)
    : rule_resolver_(rule_resolver), include_resolver_(include_resolver), resolver_(builtins) {}

void CompiledTemplateExecutor::execute(const CompiledProgram& program, const EffectiveSettings& settings,
                                       const std::filesystem::path& current_file, RenderSession& session,
                                       const ChunkSink& sink) const {
    execute_range(program, 0, program.template_instructions.size(), settings, current_file, session, sink, true);
}

void CompiledTemplateExecutor::execute_range(const CompiledProgram& program, std::size_t begin, std::size_t end,
                                             const EffectiveSettings& settings,
                                             const std::filesystem::path& current_file,
                                             RenderSession& session, const ChunkSink& sink,
                                             bool count_output) const {
    std::size_t instruction = begin;
    while (instruction < end) {
        ensure_render_time_budget(settings, current_file, session);
        const TemplateInstruction& op = program.template_instructions[instruction];
        switch (op.opcode) {
        case TemplateOpcode::EmitText:
            emit_chunk(data_view(program, op.data_offset, op.data_length), settings, current_file, session, sink, count_output);
            ++instruction;
            break;
        case TemplateOpcode::EmitExpr: {
            Value value = evaluate_expression(program, InstructionRange{op.data_offset, op.data_length}, settings,
                                              current_file, session);
            if (value.is_object() || value.is_list()) {
                throw DiagnosticError(make_exec_error("Structured values cannot be rendered directly", current_file));
            }
            emit_value(value, settings, current_file, session, sink, count_output);
            ++instruction;
            break;
        }
        case TemplateOpcode::EmitLuaExpr:
        case TemplateOpcode::EmitLuaBlock: {
            if (!session.lua_runtime) {
                session.lua_runtime = std::make_shared<LuaRuntime>();
            }
            const SourceSpan span = make_span(current_file, op.arg0, op.arg1);
            const LuaChunkMode mode = op.opcode == TemplateOpcode::EmitLuaExpr ? LuaChunkMode::InlineValue : LuaChunkMode::BlockValue;
            emit_value(session.lua_runtime->execute(std::string(data_view(program, op.data_offset, op.data_length)), mode,
                                                    settings, session, current_file, span),
                       settings, current_file, session, sink, count_output);
            ++instruction;
            break;
        }
        case TemplateOpcode::Include: {
            if (!settings.allow_includes) {
                throw DiagnosticError(make_exec_error("Includes are disabled", current_file));
            }
            ensure_include_depth_limit(settings, current_file, session);
            if (try_execute_prepared_include(program, static_cast<std::uint32_t>(instruction), settings, *this, session, sink)) {
                ++instruction;
                break;
            }
            ResolvedInclude include = include_resolver_.load(std::string(data_view(program, op.data_offset, op.data_length)),
                                                             current_file, settings, session);
            const EffectiveSettings& include_settings = include_settings_for(rule_resolver_, settings, include.logical_path, session);
            if (include.kind == ResolvedIncludeKind::Compiled && session.prepared_include_cache_ref != nullptr) {
                (*session.prepared_include_cache_ref)[RenderSession::PreparedIncludeKey{&program, static_cast<std::uint32_t>(instruction)}]
                    = RenderSession::PreparedIncludeEntry{include.compiled_program, &include_settings, include.logical_path,
                                                          std::chrono::steady_clock::now() + FileMetadataCache::ttl()};
            }
            if (include.kind == ResolvedIncludeKind::Compiled) {
                if (include.compiled_program == nullptr) {
                    throw DiagnosticError(make_exec_error("Compiled include payload missing", include.path));
                }
                session.push_local_scope();
                try {
                    execute(*include.compiled_program, include_settings, include.logical_path, session, sink);
                } catch (...) {
                    session.pop_local_scope();
                    throw;
                }
                session.pop_local_scope();
            } else {
                CompiledTemplateCompiler compiler;
                CompiledTemplateSerializer serializer;
                std::string prepared_source = std::string(include.source.view());
                if (include_settings.replace_tabs) {
                    prepared_source = text::replace_tabs(prepared_source, include_settings.tab_size);
                }
                const CompiledProgram compiled = compiler.compile_source(prepared_source, include.path,
                                                                         include.logical_path, include_settings);
                CompiledTemplateCache::instance().store_in_memory(serializer.compiled_path_for_source(include.path),
                                                                  compiled, include_settings);
                CompiledTemplateWriter::instance().enqueue(serializer.compiled_path_for_source(include.path),
                                                           serializer.serialize(compiled));
                session.push_local_scope();
                try {
                    execute(compiled, include_settings, include.logical_path, session, sink);
                } catch (...) {
                    session.pop_local_scope();
                    throw;
                }
                session.pop_local_scope();
            }
            include_resolver_.pop(session);
            ++instruction;
            break;
        }
        case TemplateOpcode::ForLoop: {
            const LoopMeta meta = find_loop_meta(program, instruction);
            if (meta.end_index == std::numeric_limits<std::size_t>::max()) {
                throw DiagnosticError(make_exec_error("Malformed compiled for loop", current_file));
            }

            const LoopBindingNames bindings = parse_loop_bindings(data_view(program, op.data_offset, op.data_length));
            const bool object_loop = bindings.has_second;
            const std::size_t body_begin = instruction + 1;
            const std::size_t body_end = meta.else_index == std::numeric_limits<std::size_t>::max() ? meta.end_index : meta.else_index;
            const std::size_t else_begin = meta.else_index == std::numeric_limits<std::size_t>::max() ? meta.end_index : meta.else_index + 1;
            const std::size_t else_end = meta.end_index;

            Value iterable = evaluate_expression(program, InstructionRange{op.arg0, op.arg1}, settings,
                                                 current_file, session);

            if (iterable.is_null()) {
                if (else_begin < else_end) {
                    execute_range(program, else_begin, else_end, settings, current_file, session, sink, count_output);
                }
                instruction = meta.end_index + 1;
                break;
            }

            bool rendered_any = false;
            if (!object_loop) {
                if (!iterable.is_list()) {
                    throw DiagnosticError(make_exec_error("For-loop iterable must be list or null", current_file));
                }
                const Value::List* items = iterable.try_as_list();
                if (items == nullptr) {
                    throw DiagnosticError(make_exec_error("For-loop iterable must be list or null", current_file));
                }
                ensure_loop_iteration_limit(settings, current_file, items->size());
                for (std::size_t index0 = 0; index0 < items->size(); ++index0) {
                    rendered_any = true;
                    session.push_loop_frame(make_loop_frame(bindings.first, Value::borrowed_data((*items)[index0]), index0, items->size()));
                    execute_range(program, body_begin, body_end, settings, current_file, session, sink, count_output);
                    session.pop_loop_frame();
                }
            } else {
                if (!iterable.is_object()) {
                    throw DiagnosticError(make_exec_error("For-loop iterable must be object or null", current_file));
                }
                const Value::Object* items = iterable.try_as_object();
                if (items == nullptr) {
                    throw DiagnosticError(make_exec_error("For-loop iterable must be object or null", current_file));
                }
                ensure_loop_iteration_limit(settings, current_file, items->size());
                std::size_t index0 = 0;
                for (const auto& [key, value] : *items) {
                    rendered_any = true;
                    session.push_loop_frame(make_loop_frame(bindings.first, Value(key), bindings.second,
                                                            Value::borrowed_data(value), index0, items->size()));
                    execute_range(program, body_begin, body_end, settings, current_file, session, sink, count_output);
                    session.pop_loop_frame();
                    ++index0;
                }
            }

            if (!rendered_any && else_begin < else_end) {
                execute_range(program, else_begin, else_end, settings, current_file, session, sink, count_output);
            }
            instruction = meta.end_index + 1;
            break;
        }
        case TemplateOpcode::ElseLoop:
        case TemplateOpcode::EndFor:
            return;
        case TemplateOpcode::PushScope:
            session.push_local_scope();
            ++instruction;
            break;
        case TemplateOpcode::PopScope:
            session.pop_local_scope();
            ++instruction;
            break;
        case TemplateOpcode::SetLocal: {
            Value value = evaluate_expression(program, InstructionRange{op.arg0, op.arg1}, settings, current_file, session);
            session.set_local_value(std::string(data_view(program, op.data_offset, op.data_length)), std::move(value));
            ++instruction;
            break;
        }
        case TemplateOpcode::DefineFunction: {
            if (op.arg0 >= program.functions.size()) {
                throw DiagnosticError(make_exec_error("Malformed compiled function definition", current_file));
            }
            const CompiledFunction& compiled = program.functions[op.arg0];
            if (session.current_function_scope_contains(compiled.name, settings.case_sensitive_variables)) {
                throw DiagnosticError(make_runtime_error("Duplicate function definition: " + compiled.name,
                                                         current_file,
                                                         static_cast<std::uint32_t>(compiled.span.start.line),
                                                         static_cast<std::uint32_t>(compiled.span.start.column)));
            }
            RenderSession::FunctionDefinition function;
            function.kind = compiled.kind == CompiledFunction::Kind::Lua
                ? RenderSession::FunctionDefinition::Kind::Lua
                : RenderSession::FunctionDefinition::Kind::Template;
            function.parameters = compiled.parameters;
            function.program = &program;
            function.body_range = compiled.body_range;
            function.lua_source = compiled.lua_source;
            function.definition_file = compiled.definition_file;
            function.definition_span = compiled.span;
            session.set_function(compiled.name, std::move(function));
            ++instruction;
            break;
        }
        case TemplateOpcode::Jump:
            instruction = op.arg0;
            break;
        case TemplateOpcode::JumpIfFalse:
            {
                const InstructionRange range{op.data_offset, op.data_length};
                const Value value = evaluate_expression(program, range, settings, current_file, session);
                if (!value.to_bool()) {
                    if (settings.error_on_false_input) {
                        const auto [line, column] = condition_location(program, range);
                        throw DiagnosticError(make_runtime_error("False input is not allowed in condition",
                                                                 current_file,
                                                                 line,
                                                                 column));
                    }
                    instruction = op.arg0;
                } else {
                    ++instruction;
                }
            }
            break;
        case TemplateOpcode::End:
            return;
        }
    }
}

Value CompiledTemplateExecutor::evaluate_expression(const CompiledProgram& program, InstructionRange range,
                                                    const EffectiveSettings& settings,
                                                    const std::filesystem::path& current_file,
                                                    RenderSession& session) const {
    ensure_render_time_budget(settings, current_file, session);
    if (range.length == 1) {
        const ExpressionInstruction& op = program.expression_instructions[range.offset];
        switch (op.opcode) {
        case ExpressionOpcode::LoadVar:
        case ExpressionOpcode::LoadBuiltin: {
            const std::string_view name = data_view(program, op.data_offset, op.data_length);
            if (op.opcode == ExpressionOpcode::LoadVar && name != "ARGS" && name.find('.') == std::string_view::npos
                && can_fast_path_variable_lookup(settings, session)) {
                if (const Value* value = session.lookup_scoped_value(name, true)) {
                    if (const auto string_value = value->try_as_string_view()) {
                        return Value::borrowed(*string_value);
                    }
                    return *value;
                }
                if (const Value* value = session.variables_view().get_value(name, true)) {
                    if (const auto string_value = value->try_as_string_view()) {
                        return Value::borrowed(*string_value);
                    }
                    return *value;
                }
                return Value();
            }
            if (op.opcode == ExpressionOpcode::LoadVar && name.find('.') != std::string_view::npos) {
                if (const auto fast_value = try_fast_loop_lookup(name, settings.case_sensitive_variables, session)) {
                    return *fast_value;
                }
            }
            const SourceSpan span = make_span(current_file, op.arg0, op.arg1);
            return resolver_.resolve_identifier(std::string(name), span, settings, session, current_file);
        }
        case ExpressionOpcode::LoadArg: {
            if (can_fast_path_arg_lookup(settings)) {
                const auto& args = session.args_view();
                if (op.arg0 < args.size()) {
                    return Value::borrowed(args[op.arg0]);
                }
            }
            const SourceSpan span = make_span(current_file, op.arg1, 1);
            return resolver_.resolve_identifier("ARGS[" + std::to_string(op.arg0) + "]", span,
                                                settings, session, current_file);
        }
        case ExpressionOpcode::PushString:
            return Value::borrowed(data_view(program, op.data_offset, op.data_length));
        case ExpressionOpcode::PushBool:
            return Value(op.arg0 != 0);
        case ExpressionOpcode::PushNumber: {
            const std::uint64_t bits = static_cast<std::uint64_t>(op.arg0) | (static_cast<std::uint64_t>(op.arg1) << 32);
            return Value(std::bit_cast<double>(bits));
        }
        case ExpressionOpcode::EvalLua: {
            if (!session.lua_runtime) {
                session.lua_runtime = std::make_shared<LuaRuntime>();
            }
            const SourceSpan span = make_span(current_file, op.arg0, op.arg1);
            return session.lua_runtime->execute(std::string(data_view(program, op.data_offset, op.data_length)),
                                                LuaChunkMode::Predicate, settings, session, current_file, span);
        }
        case ExpressionOpcode::CallFunction: {
            const std::string name(data_view(program, op.data_offset, op.data_length));
            const RenderSession::FunctionDefinition* function = session.lookup_function(name, settings.case_sensitive_variables);
            if (function == nullptr) {
                throw DiagnosticError(make_runtime_error("Unknown function: " + name, current_file, op.arg1, 1));
            }
            if (op.arg0 != 0) {
                throw DiagnosticError(make_runtime_error("Function call arity mismatch for " + name, current_file, op.arg1, 1));
            }
            return call_function(*function, {}, settings, current_file, session);
        }
        case ExpressionOpcode::LoadMember:
        case ExpressionOpcode::LoadIndex:
        case ExpressionOpcode::Len:
        case ExpressionOpcode::Eq:
        case ExpressionOpcode::Ne:
        case ExpressionOpcode::Lt:
        case ExpressionOpcode::Gt:
        case ExpressionOpcode::Le:
        case ExpressionOpcode::Ge:
        case ExpressionOpcode::In:
        case ExpressionOpcode::ApplyFilter:
        case ExpressionOpcode::Not:
        case ExpressionOpcode::ToBool:
        case ExpressionOpcode::Jump:
        case ExpressionOpcode::JumpIfFalse:
        case ExpressionOpcode::JumpIfTrue:
            break;
        }
    }

    std::vector<Value> stack;
    stack.reserve(range.length);
    std::size_t instruction = range.offset;
    const std::size_t end = range.offset + range.length;
    while (instruction < end) {
        const ExpressionInstruction& op = program.expression_instructions[instruction];
        switch (op.opcode) {
        case ExpressionOpcode::LoadVar:
        case ExpressionOpcode::LoadBuiltin: {
            const std::string_view name = data_view(program, op.data_offset, op.data_length);
            if (op.opcode == ExpressionOpcode::LoadVar && name != "ARGS" && name.find('.') == std::string_view::npos
                && can_fast_path_variable_lookup(settings, session)) {
                if (const Value* value = session.lookup_scoped_value(name, true)) {
                    if (const auto string_value = value->try_as_string_view()) {
                        stack.push_back(Value::borrowed(*string_value));
                    } else {
                        stack.push_back(*value);
                    }
                } else if (const Value* value = session.variables_view().get_value(name, true)) {
                    if (const auto string_value = value->try_as_string_view()) {
                        stack.push_back(Value::borrowed(*string_value));
                    } else {
                        stack.push_back(*value);
                    }
                } else {
                    stack.push_back(Value());
                }
            } else if (op.opcode == ExpressionOpcode::LoadVar && name.find('.') != std::string_view::npos) {
                if (const auto fast_value = try_fast_loop_lookup(name, settings.case_sensitive_variables, session)) {
                    stack.push_back(*fast_value);
                } else {
                    const SourceSpan span = make_span(current_file, op.arg0, op.arg1);
                    stack.push_back(resolver_.resolve_identifier(std::string(name), span, settings, session, current_file));
                }
            } else {
                const SourceSpan span = make_span(current_file, op.arg0, op.arg1);
                stack.push_back(resolver_.resolve_identifier(std::string(name), span, settings, session, current_file));
            }
            ++instruction;
            break;
        }
        case ExpressionOpcode::LoadArg: {
            if (can_fast_path_arg_lookup(settings)) {
                const auto& args = session.args_view();
                if (op.arg0 >= args.size()) {
                    const SourceSpan span = make_span(current_file, op.arg1, 1);
                    stack.push_back(resolver_.resolve_identifier("ARGS[" + std::to_string(op.arg0) + "]", span,
                                                                 settings, session, current_file));
                } else {
                    stack.push_back(Value::borrowed(args[op.arg0]));
                }
            } else {
                const SourceSpan span = make_span(current_file, op.arg1, 1);
                stack.push_back(resolver_.resolve_identifier("ARGS[" + std::to_string(op.arg0) + "]", span,
                                                             settings, session, current_file));
            }
            ++instruction;
            break;
        }
        case ExpressionOpcode::PushString:
            stack.push_back(Value::borrowed(data_view(program, op.data_offset, op.data_length)));
            ++instruction;
            break;
        case ExpressionOpcode::PushBool:
            stack.emplace_back(op.arg0 != 0);
            ++instruction;
            break;
        case ExpressionOpcode::PushNumber: {
            const std::uint64_t bits = static_cast<std::uint64_t>(op.arg0) | (static_cast<std::uint64_t>(op.arg1) << 32);
            stack.emplace_back(std::bit_cast<double>(bits));
            ++instruction;
            break;
        }
        case ExpressionOpcode::EvalLua: {
            if (!session.lua_runtime) {
                session.lua_runtime = std::make_shared<LuaRuntime>();
            }
            const SourceSpan span = make_span(current_file, op.arg0, op.arg1);
            stack.push_back(session.lua_runtime->execute(std::string(data_view(program, op.data_offset, op.data_length)),
                                                         LuaChunkMode::Predicate, settings, session, current_file, span));
            ++instruction;
            break;
        }
        case ExpressionOpcode::LoadMember: {
            const Value base = std::move(stack.back());
            stack.pop_back();
            const SourceSpan span = make_span(current_file, op.arg0, op.arg1);
            stack.push_back(resolver_.resolve_member(base, data_view(program, op.data_offset, op.data_length), span, settings));
            ++instruction;
            break;
        }
        case ExpressionOpcode::LoadIndex: {
            const Value index = std::move(stack.back());
            stack.pop_back();
            const Value base = std::move(stack.back());
            stack.pop_back();
            const SourceSpan span = make_span(current_file, op.arg0, op.arg1);
            stack.push_back(resolver_.resolve_index(base, index, span, settings));
            ++instruction;
            break;
        }
        case ExpressionOpcode::Len: {
            const Value value = std::move(stack.back());
            stack.pop_back();
            stack.emplace_back(static_cast<double>(value.length()));
            ++instruction;
            break;
        }
        case ExpressionOpcode::Eq: {
            const Value right = std::move(stack.back());
            stack.pop_back();
            const Value left = std::move(stack.back());
            stack.pop_back();
            ensure_scalar_comparison(left, right, current_file, op.arg0, op.arg1);
            stack.emplace_back(left.equals(right));
            ++instruction;
            break;
        }
        case ExpressionOpcode::Ne: {
            const Value right = std::move(stack.back());
            stack.pop_back();
            const Value left = std::move(stack.back());
            stack.pop_back();
            ensure_scalar_comparison(left, right, current_file, op.arg0, op.arg1);
            stack.emplace_back(!left.equals(right));
            ++instruction;
            break;
        }
        case ExpressionOpcode::Lt:
        case ExpressionOpcode::Gt:
        case ExpressionOpcode::Le:
        case ExpressionOpcode::Ge: {
            const Value right = std::move(stack.back());
            stack.pop_back();
            const Value left = std::move(stack.back());
            stack.pop_back();
            ensure_scalar_operand(left, current_file, op.arg0, op.arg1, op.opcode == ExpressionOpcode::Lt ? "<" : op.opcode == ExpressionOpcode::Gt ? ">" : op.opcode == ExpressionOpcode::Le ? "<=" : ">=");
            ensure_scalar_operand(right, current_file, op.arg0, op.arg1, op.opcode == ExpressionOpcode::Lt ? "<" : op.opcode == ExpressionOpcode::Gt ? ">" : op.opcode == ExpressionOpcode::Le ? "<=" : ">=");
            const auto ordering = left.compare_scalar(right);
            if (!ordering.has_value()) {
                throw DiagnosticError(make_runtime_error("Comparison requires scalar operands", current_file, op.arg0, op.arg1));
            }
            const std::string_view op_name = op.opcode == ExpressionOpcode::Lt ? "<" : op.opcode == ExpressionOpcode::Gt ? ">" : op.opcode == ExpressionOpcode::Le ? "<=" : ">=";
            stack.emplace_back(compare_with_order(*ordering, op_name));
            ++instruction;
            break;
        }
        case ExpressionOpcode::In: {
            const Value right = std::move(stack.back());
            stack.pop_back();
            const Value left = std::move(stack.back());
            stack.pop_back();
            ensure_scalar_operand(left, current_file, op.arg0, op.arg1, "in");
            if (!right.is_list() && !right.is_object() && !right.try_as_string_view().has_value()) {
                throw DiagnosticError(make_runtime_error("Operator 'in' requires string, list, or object on right side", current_file, op.arg0, op.arg1));
            }
            stack.emplace_back(contains_value(right, left));
            ++instruction;
            break;
        }
        case ExpressionOpcode::ApplyFilter: {
            std::vector<Value> arguments;
            arguments.reserve(static_cast<std::size_t>(op.arg0) + 1);
            for (std::size_t index = 0; index < static_cast<std::size_t>(op.arg0); ++index) {
                arguments.push_back(std::move(stack.back()));
                stack.pop_back();
            }
            std::reverse(arguments.begin(), arguments.end());
            Value input = std::move(stack.back());
            stack.pop_back();
            arguments.insert(arguments.begin(), std::move(input));
            stack.push_back(filters_.apply(data_view(program, op.data_offset, op.data_length), arguments));
            ++instruction;
            break;
        }
        case ExpressionOpcode::CallFunction: {
            std::vector<Value> arguments;
            arguments.reserve(static_cast<std::size_t>(op.arg0));
            for (std::size_t index = 0; index < static_cast<std::size_t>(op.arg0); ++index) {
                arguments.push_back(std::move(stack.back()));
                stack.pop_back();
            }
            std::reverse(arguments.begin(), arguments.end());
            const std::string name(data_view(program, op.data_offset, op.data_length));
            const RenderSession::FunctionDefinition* function = session.lookup_function(name, settings.case_sensitive_variables);
            if (function == nullptr) {
                throw DiagnosticError(make_runtime_error("Unknown function: " + name, current_file, op.arg1, 1));
            }
            stack.push_back(call_function(*function, std::move(arguments), settings, current_file, session));
            ++instruction;
            break;
        }
        case ExpressionOpcode::Not: {
            const Value value = std::move(stack.back());
            stack.pop_back();
            stack.emplace_back(!value.to_bool());
            ++instruction;
            break;
        }
        case ExpressionOpcode::ToBool: {
            const Value value = std::move(stack.back());
            stack.pop_back();
            stack.emplace_back(value.to_bool());
            ++instruction;
            break;
        }
        case ExpressionOpcode::Jump:
            instruction = op.arg0;
            break;
        case ExpressionOpcode::JumpIfFalse: {
            const Value value = std::move(stack.back());
            stack.pop_back();
            if (!value.to_bool()) {
                instruction = op.arg0;
            } else {
                ++instruction;
            }
            break;
        }
        case ExpressionOpcode::JumpIfTrue: {
            const Value value = std::move(stack.back());
            stack.pop_back();
            if (value.to_bool()) {
                instruction = op.arg0;
            } else {
                ++instruction;
            }
            break;
        }
        }
    }

    if (stack.empty()) {
        return Value();
    }
    return std::move(stack.back());
}

Value CompiledTemplateExecutor::call_function(const RenderSession::FunctionDefinition& function,
                                              std::vector<Value> arguments,
                                              const EffectiveSettings& settings,
                                              const std::filesystem::path& current_file,
                                              RenderSession& session) const {
    const RenderSession::FunctionDefinition active_function = function;
    const std::filesystem::path& function_file = function_file_for(active_function, current_file);
    ensure_render_time_budget(settings, function_file, session);
    if (active_function.parameters.size() != arguments.size()) {
        throw DiagnosticError(make_runtime_error(
            "Wrong function arity: expected " + std::to_string(active_function.parameters.size())
                + ", got " + std::to_string(arguments.size()),
            function_file,
            static_cast<std::uint32_t>(active_function.definition_span.start.line),
            static_cast<std::uint32_t>(active_function.definition_span.start.column)));
    }
    if (session.function_call_depth >= RenderSession::kMaxFunctionCallDepth) {
        throw DiagnosticError(make_runtime_error("Function recursion limit exceeded",
                                                 function_file,
                                                 static_cast<std::uint32_t>(active_function.definition_span.start.line),
                                                 static_cast<std::uint32_t>(active_function.definition_span.start.column)));
    }

    ++session.function_call_depth;
    session.push_local_scope();
    for (std::size_t index = 0; index < active_function.parameters.size(); ++index) {
        session.set_local_value(active_function.parameters[index], std::move(arguments[index]));
    }

    try {
        if (active_function.kind == RenderSession::FunctionDefinition::Kind::Lua) {
            if (!session.lua_runtime) {
                session.lua_runtime = std::make_shared<LuaRuntime>();
            }
            const Value value = session.lua_runtime->execute(std::string(active_function.lua_source),
                                                             LuaChunkMode::BlockValue,
                                                             settings,
                                                             session,
                                                             function_file,
                                                             active_function.definition_span);
            session.pop_local_scope();
            --session.function_call_depth;
            return value;
        }

        if (active_function.program == nullptr) {
            throw DiagnosticError(make_exec_error("Compiled function body missing", function_file));
        }
        std::string output;
        execute_range(*active_function.program,
                      active_function.body_range.offset,
                      active_function.body_range.offset + active_function.body_range.length,
                      settings,
                      function_file,
                      session,
                      [&](std::string_view chunk) { output.append(chunk.data(), chunk.size()); },
                      false);
        session.pop_local_scope();
        --session.function_call_depth;
        return Value(std::move(output));
    } catch (...) {
        session.pop_local_scope();
        --session.function_call_depth;
        throw;
    }
}

std::string_view CompiledTemplateExecutor::data_view(const CompiledProgram& program, std::uint32_t offset,
                                                     std::uint32_t length) const {
    return std::string_view(program.data_blob).substr(offset, length);
}

SourceSpan CompiledTemplateExecutor::make_span(const std::filesystem::path& current_file, std::uint32_t line,
                                               std::uint32_t column) const {
    SourceSpan span;
    span.file_path = current_file.string();
    span.start.line = line;
    span.start.column = column;
    span.end = span.start;
    return span;
}

}
