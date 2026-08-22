#pragma once

#include <string_view>

#include "config/ConfigTypes.h"
#include "runtime/compiled/CompiledTemplateProgram.h"

namespace prebyte {

inline bool program_has_dynamic_ops(const CompiledProgram& program) {
    for (const CompiledFunction& function : program.functions) {
        if (function.kind == CompiledFunction::Kind::Lua) {
            return true;
        }
    }

    for (const TemplateInstruction& instruction : program.template_instructions) {
        if (instruction.opcode == TemplateOpcode::EmitLuaExpr || instruction.opcode == TemplateOpcode::EmitLuaBlock) {
            return true;
        }
    }

    const std::string_view data(program.data_blob);
    for (const ExpressionInstruction& instruction : program.expression_instructions) {
        if (instruction.opcode == ExpressionOpcode::EvalLua || instruction.opcode == ExpressionOpcode::LoadArg) {
            return true;
        }
        if (instruction.opcode == ExpressionOpcode::LoadBuiltin) {
            const std::string_view name = data.substr(instruction.data_offset, instruction.data_length);
            if (name == "__TIME__" || name == "__DATE__" || name == "__TIMESTAMP__"
                || name == "__YEAR__" || name == "__MONTH__" || name == "__DAY__"
                || name == "__UNIX_EPOCH__" || name == "__USER__" || name == "__HOST__"
                || name == "__WORKING_DIR__" || name == "__UUID__" || name == "__RANDOM__") {
                return true;
            }
        }
    }

    return false;
}

inline bool can_cache_program_output(const CompiledProgram& program, const EffectiveSettings& settings) {
    return !settings.allow_env && !program_has_dynamic_ops(program);
}

}
