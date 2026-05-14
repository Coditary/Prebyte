#include "runtime/CompiledTemplateCompiler.h"

#include "runtime/FileMetadataCache.h"
#include "support/Diagnostic.h"
#include "template/lexer/TemplateLexer.h"
#include "template/parser/TemplateParser.h"

#include <bit>
#include <cmath>
#include <chrono>
#include <optional>
#include <unordered_map>

namespace prebyte {

namespace {

const std::filesystem::path& current_working_directory() {
    static const std::filesystem::path cwd = []() {
        std::error_code error;
        const std::filesystem::path path = std::filesystem::current_path(error);
        return error ? std::filesystem::path{} : path;
    }();
    return cwd;
}

struct DataSlice {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

std::filesystem::path canonical_path(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }
    if (path.is_absolute()) {
        return path.lexically_normal();
    }
    if (current_working_directory().empty()) {
        return path.lexically_normal();
    }
    return (current_working_directory() / path).lexically_normal();
}

std::int64_t file_mtime_ticks(const std::filesystem::path& path) {
    if (path.empty()) {
        return 0;
    }
    const FileMetadata metadata = FileMetadataCache::instance().probe(path);
    if (!metadata.exists) {
        return 0;
    }
    return metadata.mtime_ticks;
}

std::optional<std::uint32_t> constant_arg_index(const ExpressionNode& expression) {
    if (expression.kind != ExpressionKind::IndexAccess) {
        return std::nullopt;
    }

    const auto& index_access = static_cast<const IndexAccessExpr&>(expression);
    if (index_access.base == nullptr || index_access.base->kind != ExpressionKind::Identifier) {
        return std::nullopt;
    }
    const auto& identifier = static_cast<const IdentifierExpr&>(*index_access.base);
    if (identifier.name != "ARGS") {
        return std::nullopt;
    }
    if (index_access.index == nullptr || index_access.index->kind != ExpressionKind::Number) {
        return std::nullopt;
    }

    const auto& number = static_cast<const NumberExpr&>(*index_access.index);
    const double value = number.value;
    if (value < 0 || std::floor(value) != value) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(value);
}

std::optional<std::string> flatten_identifier_path(const ExpressionNode& expression) {
    if (expression.kind == ExpressionKind::Identifier) {
        return static_cast<const IdentifierExpr&>(expression).name;
    }
    if (expression.kind != ExpressionKind::MemberAccess) {
        return std::nullopt;
    }

    const auto& member = static_cast<const MemberAccessExpr&>(expression);
    if (member.base == nullptr) {
        return std::nullopt;
    }

    auto path = flatten_identifier_path(*member.base);
    if (!path.has_value()) {
        return std::nullopt;
    }
    path->append(".");
    path->append(member.member);
    return path;
}

void append_span(ExpressionInstruction& instruction, const SourceSpan& span) {
    instruction.arg0 = static_cast<std::uint32_t>(span.start.line);
    instruction.arg1 = static_cast<std::uint32_t>(span.start.column);
}

bool is_builtin_name(const std::string& name) {
    return name == "__TIME__" || name == "__LINE__" || name == "__FILE__"
        || name == "__FILENAME__" || name == "__DIR__" || name == "__EXTENSION__"
        || name == "__DATE__" || name == "__TIMESTAMP__" || name == "__YEAR__"
        || name == "__MONTH__" || name == "__DAY__" || name == "__UNIX_EPOCH__"
        || name == "__USER__" || name == "__HOST__" || name == "__OS__"
        || name == "__WORKING_DIR__" || name == "__UUID__" || name == "__RANDOM__";
}

class CompilerBuilder {
public:
    CompilerBuilder(const std::filesystem::path& source_path, const std::filesystem::path& logical_path,
                    const EffectiveSettings& settings) {
        program_.source_path = canonical_path(source_path);
        program_.logical_path = logical_path.empty() ? program_.source_path : logical_path;
        program_.parse_options.variable_prefix = settings.variable_prefix;
        program_.parse_options.variable_suffix = settings.variable_suffix;
        program_.parse_options.replace_tabs = settings.replace_tabs;
        program_.parse_options.tab_size = static_cast<std::uint32_t>(settings.tab_size);
        if (!program_.source_path.empty()) {
            program_.dependencies.push_back(CompiledDependency{program_.source_path, file_mtime_ticks(program_.source_path)});
        }
    }

    CompiledProgram build(const DocumentNode& document) {
        compile_nodes(document.children);
        emit_template(TemplateInstruction{.opcode = TemplateOpcode::End});
        return std::move(program_);
    }

private:
    DataSlice store_data(std::string_view value) {
        const auto it = data_slices_.find(std::string(value));
        if (it != data_slices_.end()) {
            return it->second;
        }

        DataSlice slice;
        slice.offset = static_cast<std::uint32_t>(program_.data_blob.size());
        slice.length = static_cast<std::uint32_t>(value.size());
        program_.data_blob.append(value.data(), value.size());
        data_slices_.emplace(std::string(value), slice);
        return slice;
    }

    std::uint32_t emit_template(TemplateInstruction instruction) {
        program_.template_instructions.push_back(instruction);
        return static_cast<std::uint32_t>(program_.template_instructions.size() - 1);
    }

    std::uint32_t emit_expression(ExpressionInstruction instruction) {
        program_.expression_instructions.push_back(instruction);
        return static_cast<std::uint32_t>(program_.expression_instructions.size() - 1);
    }

    InstructionRange compile_expression(const ExpressionNode& expression) {
        const std::uint32_t start = static_cast<std::uint32_t>(program_.expression_instructions.size());
        compile_expression_node(expression);
        return InstructionRange{start, static_cast<std::uint32_t>(program_.expression_instructions.size() - start)};
    }

    void compile_expression_node(const ExpressionNode& expression) {
        switch (expression.kind) {
        case ExpressionKind::Identifier: {
            const auto& identifier = static_cast<const IdentifierExpr&>(expression);
            const DataSlice slice = store_data(identifier.name);
            emit_expression(ExpressionInstruction{
                .opcode = is_builtin_name(identifier.name) ? ExpressionOpcode::LoadBuiltin : ExpressionOpcode::LoadVar,
                .data_offset = slice.offset,
                .data_length = slice.length,
                .arg0 = static_cast<std::uint32_t>(identifier.span.start.line),
                .arg1 = static_cast<std::uint32_t>(identifier.span.start.column),
            });
            return;
        }
        case ExpressionKind::String: {
            const auto& string_expr = static_cast<const StringExpr&>(expression);
            const DataSlice slice = store_data(string_expr.value);
            emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::PushString,
                                                 .data_offset = slice.offset,
                                                 .data_length = slice.length});
            return;
        }
        case ExpressionKind::Number: {
            const auto bits = std::bit_cast<std::uint64_t>(static_cast<const NumberExpr&>(expression).value);
            emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::PushNumber,
                                                 .arg0 = static_cast<std::uint32_t>(bits & 0xffffffffu),
                                                 .arg1 = static_cast<std::uint32_t>(bits >> 32)});
            return;
        }
        case ExpressionKind::Bool:
            emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::PushBool,
                                                 .arg0 = static_cast<const BoolExpr&>(expression).value ? 1u : 0u});
            return;
        case ExpressionKind::Unary: {
            const auto& unary = static_cast<const UnaryExpr&>(expression);
            compile_expression_node(*unary.operand);
            emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::Not});
            return;
        }
        case ExpressionKind::Binary: {
            const auto& binary = static_cast<const BinaryExpr&>(expression);
            if (binary.op == "==" || binary.op == "!=" || binary.op == "<" || binary.op == ">"
                || binary.op == "<=" || binary.op == ">=" || binary.op == "in") {
                compile_expression_node(*binary.left);
                compile_expression_node(*binary.right);
                ExpressionOpcode opcode = ExpressionOpcode::Eq;
                if (binary.op == "!=") {
                    opcode = ExpressionOpcode::Ne;
                } else if (binary.op == "<") {
                    opcode = ExpressionOpcode::Lt;
                } else if (binary.op == ">") {
                    opcode = ExpressionOpcode::Gt;
                } else if (binary.op == "<=") {
                    opcode = ExpressionOpcode::Le;
                } else if (binary.op == ">=") {
                    opcode = ExpressionOpcode::Ge;
                } else if (binary.op == "in") {
                    opcode = ExpressionOpcode::In;
                }
                ExpressionInstruction instruction{.opcode = opcode};
                append_span(instruction, expression.span);
                emit_expression(instruction);
                return;
            }
            if (binary.op == "&&") {
                compile_expression_node(*binary.left);
                emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::JumpIfFalse});
                const std::uint32_t short_jump = static_cast<std::uint32_t>(program_.expression_instructions.size() - 1);
                compile_expression_node(*binary.right);
                emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::ToBool});
                const std::uint32_t end_jump = emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::Jump});
                const std::uint32_t false_index = emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::PushBool, .arg0 = 0});
                program_.expression_instructions[short_jump].arg0 = false_index;
                const std::uint32_t end_index = static_cast<std::uint32_t>(program_.expression_instructions.size());
                program_.expression_instructions[end_jump].arg0 = end_index;
                return;
            }
            if (binary.op == "||") {
                compile_expression_node(*binary.left);
                emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::JumpIfTrue});
                const std::uint32_t short_jump = static_cast<std::uint32_t>(program_.expression_instructions.size() - 1);
                compile_expression_node(*binary.right);
                emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::ToBool});
                const std::uint32_t end_jump = emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::Jump});
                const std::uint32_t true_index = emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::PushBool, .arg0 = 1});
                program_.expression_instructions[short_jump].arg0 = true_index;
                const std::uint32_t end_index = static_cast<std::uint32_t>(program_.expression_instructions.size());
                program_.expression_instructions[end_jump].arg0 = end_index;
                return;
            }

            Diagnostic diagnostic;
            diagnostic.code = "COMPILE001";
            diagnostic.message = "Unsupported binary operator: " + binary.op;
            diagnostic.span = expression.span;
            throw DiagnosticError(diagnostic);
        }
        case ExpressionKind::Grouped:
            compile_expression_node(*static_cast<const GroupedExpr&>(expression).expression);
            return;
        case ExpressionKind::MemberAccess: {
            if (const auto path = flatten_identifier_path(expression)) {
                const DataSlice slice = store_data(*path);
                emit_expression(ExpressionInstruction{
                    .opcode = is_builtin_name(*path) ? ExpressionOpcode::LoadBuiltin : ExpressionOpcode::LoadVar,
                    .data_offset = slice.offset,
                    .data_length = slice.length,
                    .arg0 = static_cast<std::uint32_t>(expression.span.start.line),
                    .arg1 = static_cast<std::uint32_t>(expression.span.start.column),
                });
                return;
            }

            const auto& member = static_cast<const MemberAccessExpr&>(expression);
            compile_expression_node(*member.base);
            const DataSlice slice = store_data(member.member);
            ExpressionInstruction instruction{.opcode = ExpressionOpcode::LoadMember,
                                              .data_offset = slice.offset,
                                              .data_length = slice.length};
            append_span(instruction, expression.span);
            emit_expression(instruction);
            return;
        }
        case ExpressionKind::IndexAccess: {
            if (const auto arg_index = constant_arg_index(expression)) {
                emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::LoadArg,
                                                     .arg0 = *arg_index,
                                                     .arg1 = static_cast<std::uint32_t>(expression.span.start.line)});
                return;
            }

            const auto& index_access = static_cast<const IndexAccessExpr&>(expression);
            if (index_access.base != nullptr && index_access.base->kind == ExpressionKind::Identifier
                && static_cast<const IdentifierExpr&>(*index_access.base).name == "ARGS") {
                Diagnostic diagnostic;
                diagnostic.code = "COMPILE001";
                diagnostic.message = "ARGS must be accessed as ARGS[index] with a constant non-negative integer";
                diagnostic.span = expression.span;
                throw DiagnosticError(diagnostic);
            }

            compile_expression_node(*index_access.base);
            compile_expression_node(*index_access.index);
            ExpressionInstruction instruction{.opcode = ExpressionOpcode::LoadIndex};
            append_span(instruction, expression.span);
            emit_expression(instruction);
            return;
        }
        case ExpressionKind::LenCall: {
            const auto& len_call = static_cast<const LenCallExpr&>(expression);
            compile_expression_node(*len_call.operand);
            emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::Len});
            return;
        }
        case ExpressionKind::LuaCall: {
            const auto& lua_call = static_cast<const LuaCallExpr&>(expression);
            const DataSlice slice = store_data(lua_call.source);
            emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::EvalLua,
                                                  .data_offset = slice.offset,
                                                  .data_length = slice.length,
                                                  .arg0 = static_cast<std::uint32_t>(expression.span.start.line),
                                                  .arg1 = static_cast<std::uint32_t>(expression.span.start.column)});
            return;
        }
        case ExpressionKind::FilterCall: {
            const auto& filter = static_cast<const FilterCallExpr&>(expression);
            compile_expression_node(*filter.input);
            for (const auto& argument : filter.arguments) {
                compile_expression_node(*argument);
            }
            const DataSlice slice = store_data(filter.name);
            emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::ApplyFilter,
                                                  .data_offset = slice.offset,
                                                  .data_length = slice.length,
                                                  .arg0 = static_cast<std::uint32_t>(filter.arguments.size()),
                                                   .arg1 = static_cast<std::uint32_t>(expression.span.start.line)});
            return;
        }
        case ExpressionKind::FunctionCall: {
            const auto& call = static_cast<const FunctionCallExpr&>(expression);
            for (const auto& argument : call.arguments) {
                compile_expression_node(*argument);
            }
            const DataSlice slice = store_data(call.name);
            emit_expression(ExpressionInstruction{.opcode = ExpressionOpcode::CallFunction,
                                                  .data_offset = slice.offset,
                                                  .data_length = slice.length,
                                                  .arg0 = static_cast<std::uint32_t>(call.arguments.size()),
                                                  .arg1 = static_cast<std::uint32_t>(expression.span.start.line)});
            return;
        }
        }
    }

    void compile_nodes(const std::vector<TemplateNodePtr>& nodes) {
        for (const auto& node : nodes) {
            compile_node(*node);
        }
    }

    void compile_node(const TemplateNode& node) {
        switch (node.kind) {
        case TemplateNodeKind::Text: {
            const auto& text_node = static_cast<const TextNode&>(node);
            const DataSlice slice = store_data(text_node.text);
            emit_template(TemplateInstruction{.opcode = TemplateOpcode::EmitText,
                                              .data_offset = slice.offset,
                                              .data_length = slice.length});
            return;
        }
        case TemplateNodeKind::Interpolation: {
            const auto& interpolation = static_cast<const InterpolationNode&>(node);
            const InstructionRange range = compile_expression(*interpolation.expression);
            emit_template(TemplateInstruction{.opcode = TemplateOpcode::EmitExpr,
                                              .data_offset = range.offset,
                                              .data_length = range.length,
                                              .arg0 = static_cast<std::uint32_t>(node.span.start.line),
                                              .arg1 = static_cast<std::uint32_t>(node.span.start.column)});
            return;
        }
        case TemplateNodeKind::LuaExpr: {
            const auto& lua_node = static_cast<const LuaExprNode&>(node);
            const DataSlice slice = store_data(lua_node.source);
            emit_template(TemplateInstruction{.opcode = TemplateOpcode::EmitLuaExpr,
                                              .data_offset = slice.offset,
                                              .data_length = slice.length,
                                              .arg0 = static_cast<std::uint32_t>(node.span.start.line),
                                              .arg1 = static_cast<std::uint32_t>(node.span.start.column)});
            return;
        }
        case TemplateNodeKind::LuaBlock: {
            const auto& lua_node = static_cast<const LuaBlockNode&>(node);
            const DataSlice slice = store_data(lua_node.source);
            emit_template(TemplateInstruction{.opcode = TemplateOpcode::EmitLuaBlock,
                                              .data_offset = slice.offset,
                                              .data_length = slice.length,
                                              .arg0 = static_cast<std::uint32_t>(node.span.start.line),
                                              .arg1 = static_cast<std::uint32_t>(node.span.start.column)});
            return;
        }
        case TemplateNodeKind::Include: {
            const auto& include_node = static_cast<const IncludeNode&>(node);
            const DataSlice slice = store_data(include_node.path);
            emit_template(TemplateInstruction{.opcode = TemplateOpcode::Include,
                                              .data_offset = slice.offset,
                                              .data_length = slice.length,
                                              .arg0 = static_cast<std::uint32_t>(node.span.start.line),
                                               .arg1 = static_cast<std::uint32_t>(node.span.start.column)});
            return;
        }
        case TemplateNodeKind::For: {
            const auto& for_node = static_cast<const ForNode&>(node);
            const InstructionRange range = compile_expression(*for_node.iterable);
            std::string bindings = for_node.value_name;
            if (for_node.key_name.has_value()) {
                bindings = *for_node.key_name;
                bindings.push_back('\0');
                bindings += for_node.value_name;
            }
            const DataSlice slice = store_data(bindings);
            emit_template(TemplateInstruction{.opcode = TemplateOpcode::ForLoop,
                                              .data_offset = slice.offset,
                                              .data_length = slice.length,
                                              .arg0 = range.offset,
                                              .arg1 = range.length});
            emit_template(TemplateInstruction{.opcode = TemplateOpcode::PushScope});
            compile_nodes(for_node.body);
            emit_template(TemplateInstruction{.opcode = TemplateOpcode::PopScope});
            if (!for_node.else_body.empty()) {
                emit_template(TemplateInstruction{.opcode = TemplateOpcode::ElseLoop});
                emit_template(TemplateInstruction{.opcode = TemplateOpcode::PushScope});
                compile_nodes(for_node.else_body);
                emit_template(TemplateInstruction{.opcode = TemplateOpcode::PopScope});
            }
            emit_template(TemplateInstruction{.opcode = TemplateOpcode::EndFor});
            return;
        }
        case TemplateNodeKind::If: {
            const auto& if_node = static_cast<const IfNode&>(node);
            std::vector<std::uint32_t> end_jumps;
            for (std::size_t index = 0; index < if_node.branches.size(); ++index) {
                const IfBranch& branch = if_node.branches[index];
                std::optional<std::uint32_t> false_jump;
                if (branch.condition) {
                    const InstructionRange range = compile_expression(*branch.condition);
                    false_jump = emit_template(TemplateInstruction{.opcode = TemplateOpcode::JumpIfFalse,
                                                                   .data_offset = range.offset,
                                                                   .data_length = range.length});
                }

                emit_template(TemplateInstruction{.opcode = TemplateOpcode::PushScope});
                compile_nodes(branch.body);
                emit_template(TemplateInstruction{.opcode = TemplateOpcode::PopScope});
                if (index + 1 < if_node.branches.size()) {
                    end_jumps.push_back(emit_template(TemplateInstruction{.opcode = TemplateOpcode::Jump}));
                }

                if (false_jump.has_value()) {
                    program_.template_instructions[*false_jump].arg0 = static_cast<std::uint32_t>(program_.template_instructions.size());
                }
            }

            const std::uint32_t end_index = static_cast<std::uint32_t>(program_.template_instructions.size());
            for (const std::uint32_t jump : end_jumps) {
                program_.template_instructions[jump].arg0 = end_index;
            }
            return;
        }
        case TemplateNodeKind::Set: {
            const auto& set_node = static_cast<const SetNode&>(node);
            const InstructionRange range = compile_expression(*set_node.expression);
            const DataSlice slice = store_data(set_node.name);
            emit_template(TemplateInstruction{.opcode = TemplateOpcode::SetLocal,
                                              .data_offset = slice.offset,
                                              .data_length = slice.length,
                                              .arg0 = range.offset,
                                               .arg1 = range.length});
            return;
        }
        case TemplateNodeKind::FunctionDef: {
            const auto& function_node = static_cast<const FunctionDefNode&>(node);
            CompiledFunction function;
            function.kind = function_node.mode == FunctionMode::Lua ? CompiledFunction::Kind::Lua : CompiledFunction::Kind::Template;
            function.name = function_node.name;
            function.parameters = function_node.parameters;
            function.definition_file = program_.logical_path;
            function.span = node.span;

            const std::uint32_t function_index = static_cast<std::uint32_t>(program_.functions.size());
            program_.functions.push_back(function);
            emit_template(TemplateInstruction{.opcode = TemplateOpcode::DefineFunction,
                                              .arg0 = function_index});
            if (function_node.mode == FunctionMode::Lua) {
                program_.functions[function_index].lua_source = function_node.lua_source;
                return;
            }

            const std::uint32_t jump_over_body = emit_template(TemplateInstruction{.opcode = TemplateOpcode::Jump});
            const std::uint32_t body_start = static_cast<std::uint32_t>(program_.template_instructions.size());
            compile_nodes(function_node.body);
            const std::uint32_t body_end = static_cast<std::uint32_t>(program_.template_instructions.size());
            program_.functions[function_index].body_range = InstructionRange{body_start, body_end - body_start};
            program_.template_instructions[jump_over_body].arg0 = body_end;
            return;
        }
        case TemplateNodeKind::Document:
            compile_nodes(static_cast<const DocumentNode&>(node).children);
            return;
        }
    }

    CompiledProgram program_;
    std::unordered_map<std::string, DataSlice> data_slices_;
};

}

CompiledProgram CompiledTemplateCompiler::compile_source(std::string_view source,
                                                         const std::filesystem::path& source_path,
                                                         const std::filesystem::path& logical_path,
                                                         const EffectiveSettings& settings) const {
    TemplateLexer lexer(source, logical_path.string(), settings.variable_prefix, settings.variable_suffix);
    TemplateParser parser(lexer.lex(), TemplateParserOptions{.enable_loops = true});
    std::unique_ptr<DocumentNode> document = parser.parse_document();
    return compile_document(*document, source_path, logical_path, settings);
}

CompiledProgram CompiledTemplateCompiler::compile_document(const DocumentNode& document,
                                                           const std::filesystem::path& source_path,
                                                           const std::filesystem::path& logical_path,
                                                           const EffectiveSettings& settings) const {
    return CompilerBuilder(source_path, logical_path, settings).build(document);
}

}
