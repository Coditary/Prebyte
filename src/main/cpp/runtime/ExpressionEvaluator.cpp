#include "runtime/ExpressionEvaluator.h"

#include "runtime/LuaExpressionEngine.h"
#include "support/Diagnostic.h"
#include "support/TextUtil.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace prebyte {

namespace {

Diagnostic make_runtime_error(const std::string& message, const SourceSpan& span) {
    Diagnostic diagnostic;
    diagnostic.code = "RUNTIME001";
    diagnostic.message = message;
    diagnostic.span = span;
    return diagnostic;
}

std::optional<std::string> resolve_render_arg(const IdentifierExpr& expression, const RenderSession& session) {
    if (!text::starts_with(expression.name, "ARGS")) {
        return std::nullopt;
    }

    if (expression.name == "ARGS") {
        throw DiagnosticError(make_runtime_error("ARGS must be accessed as ARGS[index]", expression.span));
    }
    if (!text::starts_with(expression.name, "ARGS[") || !text::ends_with(expression.name, "]")) {
        throw DiagnosticError(make_runtime_error("Invalid ARGS reference: " + expression.name + ". Use ARGS[index]",
                                                 expression.span));
    }

    const std::string_view index_text(expression.name.data() + 5, expression.name.size() - 6);
    if (index_text.empty() || !std::all_of(index_text.begin(), index_text.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        })) {
        throw DiagnosticError(make_runtime_error("Invalid ARGS index in " + expression.name, expression.span));
    }

    const std::size_t index = static_cast<std::size_t>(std::stoull(std::string(index_text)));
    if (index >= session.args.size()) {
        throw DiagnosticError(make_runtime_error("ARGS index out of range: " + expression.name, expression.span));
    }

    return session.args[index];
}

}

ExpressionEvaluator::ExpressionEvaluator(const BuiltinRegistry& builtins)
    : builtins_(builtins) {}

Value ExpressionEvaluator::evaluate(const ExpressionNode& expression, const EffectiveSettings& settings,
                                    const RenderSession& session, const std::filesystem::path& current_file) const {
    switch (expression.kind) {
    case ExpressionKind::Identifier:
        return evaluate_identifier(static_cast<const IdentifierExpr&>(expression), settings, session, current_file);
    case ExpressionKind::String:
        return Value(static_cast<const StringExpr&>(expression).value);
    case ExpressionKind::Number:
        return Value(static_cast<const NumberExpr&>(expression).value);
    case ExpressionKind::Bool:
        return Value(static_cast<const BoolExpr&>(expression).value);
    case ExpressionKind::LuaCall: {
        LuaExpressionEngine lua_engine;
        return lua_engine.evaluate(expression, settings, session, current_file);
    }
    case ExpressionKind::Unary: {
        const auto& unary = static_cast<const UnaryExpr&>(expression);
        Value operand = evaluate(*unary.operand, settings, session, current_file);
        if (unary.op == "!") {
            return Value(!operand.to_bool());
        }
        throw DiagnosticError(make_runtime_error("Unsupported unary operator: " + unary.op, expression.span));
    }
    case ExpressionKind::Binary: {
        const auto& binary = static_cast<const BinaryExpr&>(expression);
        Value left = evaluate(*binary.left, settings, session, current_file);

        if (binary.op == "&&") {
            if (!left.to_bool()) {
                return Value(false);
            }
            Value right = evaluate(*binary.right, settings, session, current_file);
            return Value(left.to_bool() && right.to_bool());
        }
        if (binary.op == "||") {
            if (left.to_bool()) {
                return Value(true);
            }
            Value right = evaluate(*binary.right, settings, session, current_file);
            return Value(left.to_bool() || right.to_bool());
        }
        Value right = evaluate(*binary.right, settings, session, current_file);
        if (binary.op == "==") {
            return Value(left.to_string() == right.to_string());
        }
        if (binary.op == "!=") {
            return Value(left.to_string() != right.to_string());
        }
        throw DiagnosticError(make_runtime_error("Unsupported binary operator: " + binary.op, expression.span));
    }
    case ExpressionKind::Grouped:
        return evaluate(*static_cast<const GroupedExpr&>(expression).expression, settings, session, current_file);
    }

    throw DiagnosticError(make_runtime_error("Unsupported expression node", expression.span));
}

Value ExpressionEvaluator::evaluate_identifier(const IdentifierExpr& expression, const EffectiveSettings& settings,
                                               const RenderSession& session,
                                               const std::filesystem::path& current_file) const {
    if (session.ignore_names.contains(expression.name)) {
        return Value(std::string());
    }

    if (const auto render_arg = resolve_render_arg(expression, session)) {
        return Value(normalize_string(*render_arg, settings));
    }

    if (const auto builtin_value = builtins_.resolve(expression.name, expression.span, current_file)) {
        return Value(normalize_string(*builtin_value, settings));
    }

    if (const auto variable = session.variables.get(expression.name, settings.case_sensitive_variables)) {
        return Value(normalize_string(*variable, settings));
    }

    if (settings.allow_env) {
        if (const char* env = std::getenv(expression.name.c_str())) {
            return Value(normalize_string(env, settings));
        }
    }

    if (!settings.strict_variables && settings.default_variable_value.has_value()) {
        return Value(normalize_string(*settings.default_variable_value, settings));
    }

    if (!settings.strict_variables) {
        return Value(std::string());
    }

    throw DiagnosticError(make_runtime_error("Unknown variable: " + expression.name, expression.span));
}

std::string ExpressionEvaluator::normalize_string(std::string value, const EffectiveSettings& settings) const {
    if (settings.trim) {
        value = text::trim(std::move(value));
    }
    if (settings.has_max_variable_length && value.size() > settings.max_variable_length) {
        value.resize(settings.max_variable_length);
    }
    return value;
}

}
