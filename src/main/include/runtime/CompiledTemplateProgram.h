#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "support/SourceSpan.h"

namespace prebyte {

// Bump when serialized instruction/opcode layout changes.
constexpr std::uint32_t kCompiledTemplateFormatVersion = 6;

enum class TemplateOpcode : std::uint32_t {
    EmitText,
    EmitExpr,
    EmitLuaExpr,
    EmitLuaBlock,
    Include,
    ForLoop,
    ElseLoop,
    EndFor,
    PushScope,
    PopScope,
    SetLocal,
    DefineFunction,
    Jump,
    JumpIfFalse,
    End,
};

enum class ExpressionOpcode : std::uint32_t {
    LoadVar,
    LoadArg,
    LoadBuiltin,
    PushString,
    PushBool,
    PushNumber,
    EvalLua,
    LoadMember,
    LoadIndex,
    Len,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    In,
    ApplyFilter,
    CallFunction,
    Not,
    ToBool,
    Jump,
    JumpIfFalse,
    JumpIfTrue,
};

struct TemplateInstruction {
    TemplateOpcode opcode = TemplateOpcode::End;
    std::uint32_t data_offset = 0;
    std::uint32_t data_length = 0;
    std::uint32_t arg0 = 0;
    std::uint32_t arg1 = 0;
};

struct ExpressionInstruction {
    ExpressionOpcode opcode = ExpressionOpcode::PushBool;
    std::uint32_t data_offset = 0;
    std::uint32_t data_length = 0;
    std::uint32_t arg0 = 0;
    std::uint32_t arg1 = 0;
};

struct InstructionRange {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

struct CompiledFunction {
    enum class Kind : std::uint32_t {
        Template,
        Lua,
    };

    Kind kind = Kind::Template;
    std::string name;
    std::vector<std::string> parameters;
    InstructionRange body_range;
    std::string lua_source;
    std::filesystem::path definition_file;
    SourceSpan span;
};

struct CompiledDependency {
    std::filesystem::path path;
    std::int64_t mtime_ticks = 0;
};

struct CompiledParseOptions {
    std::string variable_prefix = "{{";
    std::string variable_suffix = "}}";
    bool replace_tabs = false;
    std::uint32_t tab_size = 4;
};

struct CompiledProgram {
    std::filesystem::path logical_path;
    std::filesystem::path source_path;
    CompiledParseOptions parse_options;
    std::vector<TemplateInstruction> template_instructions;
    std::vector<ExpressionInstruction> expression_instructions;
    std::string data_blob;
    std::vector<CompiledDependency> dependencies;
    std::vector<CompiledFunction> functions;
};

}
