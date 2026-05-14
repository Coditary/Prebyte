#include "runtime/LuaExpressionEngine.h"

#include "support/Diagnostic.h"

namespace prebyte {

Value LuaExpressionEngine::evaluate(const ExpressionNode& expression, const EffectiveSettings& settings,
                                    RenderSession& session, const std::filesystem::path& current_file) const {
    if (expression.kind != ExpressionKind::LuaCall) {
        Diagnostic diagnostic;
        diagnostic.code = "LUA002";
        diagnostic.message = "LuaExpressionEngine received non-Lua expression";
        diagnostic.span = expression.span;
        throw DiagnosticError(diagnostic);
    }

    if (!session.lua_runtime) {
        session.lua_runtime = std::make_shared<LuaRuntime>();
    }

    const auto& lua_expression = static_cast<const LuaCallExpr&>(expression);
    return session.lua_runtime->execute(lua_expression.source, LuaChunkMode::Predicate, settings, session, current_file,
                                        expression.span);
}

}
