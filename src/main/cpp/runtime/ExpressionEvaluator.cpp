#include "runtime/ExpressionEvaluator.h"

#include "config/RuleResolver.h"
#include "runtime/CompiledTemplateExecutor.h"
#include "runtime/IncludeResolver.h"
#include "runtime/LuaExpressionEngine.h"
#include "support/Diagnostic.h"

#include <compare>
#include <cmath>

namespace prebyte {

namespace {

Diagnostic make_runtime_error(const std::string& message, const SourceSpan& span) {
    Diagnostic diagnostic;
    diagnostic.code = "RUNTIME001";
    diagnostic.message = message;
    diagnostic.span = span;
    return diagnostic;
}

void ensure_scalar_comparison(const Value& left, const Value& right, const SourceSpan& span) {
    if (left.is_object() || left.is_list() || right.is_object() || right.is_list()) {
        throw DiagnosticError(make_runtime_error("Cannot compare structured values directly", span));
    }
}

void ensure_scalar_operand(const Value& value, const SourceSpan& span, std::string_view op_name) {
    if (value.is_object() || value.is_list()) {
        throw DiagnosticError(make_runtime_error("Operator '" + std::string(op_name) + "' requires scalar operands", span));
    }
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

Value call_function(const BuiltinRegistry& builtins, const RenderSession::FunctionDefinition& function,
                    std::vector<Value> arguments, const EffectiveSettings& settings,
                    const std::filesystem::path& current_file, RenderSession& session) {
    RuleResolver rule_resolver;
    IncludeResolver include_resolver;
    CompiledTemplateExecutor executor(rule_resolver, include_resolver, builtins);
    return executor.call_function(function, std::move(arguments), settings, current_file, session);
}

std::optional<std::uint32_t> constant_args_index(const ExpressionNode& expression) {
    if (expression.kind != ExpressionKind::IndexAccess) {
        return std::nullopt;
    }
    const auto& index_access = static_cast<const IndexAccessExpr&>(expression);
    if (index_access.base == nullptr || index_access.base->kind != ExpressionKind::Identifier) {
        return std::nullopt;
    }
    if (static_cast<const IdentifierExpr&>(*index_access.base).name != "ARGS") {
        return std::nullopt;
    }
    if (index_access.index == nullptr || index_access.index->kind != ExpressionKind::Number) {
        return std::nullopt;
    }

    const double value = static_cast<const NumberExpr&>(*index_access.index).value;
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

}

ExpressionEvaluator::ExpressionEvaluator(const BuiltinRegistry& builtins)
    : builtins_(builtins), resolver_(builtins) {}

const BuiltinRegistry& ExpressionEvaluator::builtins() const {
    return builtins_;
}

Value ExpressionEvaluator::evaluate(const ExpressionNode& expression, const EffectiveSettings& settings,
                                    RenderSession& session, const std::filesystem::path& current_file) const {
    if (session.render_time_exceeded(settings)) {
        throw DiagnosticError(make_runtime_error("Render time limit exceeded", expression.span));
    }
    switch (expression.kind) {
    case ExpressionKind::Identifier:
        return resolver_.resolve_identifier(static_cast<const IdentifierExpr&>(expression).name, expression.span,
                                            settings, session, current_file);
    case ExpressionKind::String:
        return Value(static_cast<const StringExpr&>(expression).value);
    case ExpressionKind::Number:
        return Value(static_cast<const NumberExpr&>(expression).value);
    case ExpressionKind::Bool:
        return Value(static_cast<const BoolExpr&>(expression).value);
    case ExpressionKind::MemberAccess: {
        if (const auto path = flatten_identifier_path(expression)) {
            return resolver_.resolve_identifier(*path, expression.span, settings, session, current_file);
        }
        const auto& member = static_cast<const MemberAccessExpr&>(expression);
        Value base = evaluate(*member.base, settings, session, current_file);
        return resolver_.resolve_member(base, member.member, expression.span, settings);
    }
    case ExpressionKind::IndexAccess: {
        if (const auto arg_index = constant_args_index(expression)) {
            return resolver_.resolve_identifier("ARGS[" + std::to_string(*arg_index) + "]",
                                               expression.span, settings, session, current_file);
        }
        const auto& index_access = static_cast<const IndexAccessExpr&>(expression);
        if (index_access.base != nullptr && index_access.base->kind == ExpressionKind::Identifier
            && static_cast<const IdentifierExpr&>(*index_access.base).name == "ARGS") {
            throw DiagnosticError(make_runtime_error("ARGS must be accessed as ARGS[index]", expression.span));
        }
        Value base = evaluate(*index_access.base, settings, session, current_file);
        Value index = evaluate(*index_access.index, settings, session, current_file);
        return resolver_.resolve_index(base, index, expression.span, settings);
    }
    case ExpressionKind::LenCall: {
        const auto& len_call = static_cast<const LenCallExpr&>(expression);
        Value value = evaluate(*len_call.operand, settings, session, current_file);
        return Value(static_cast<double>(value.length()));
    }
    case ExpressionKind::LuaCall: {
        LuaExpressionEngine lua_engine;
        return lua_engine.evaluate(expression, settings, session, current_file);
    }
    case ExpressionKind::FunctionCall: {
        const auto& call = static_cast<const FunctionCallExpr&>(expression);
        std::vector<Value> arguments;
        arguments.reserve(call.arguments.size());
        for (const auto& argument : call.arguments) {
            arguments.push_back(evaluate(*argument, settings, session, current_file));
        }
        const RenderSession::FunctionDefinition* function = session.lookup_function(call.name, settings.case_sensitive_variables);
        if (function == nullptr) {
            throw DiagnosticError(make_runtime_error("Unknown function: " + call.name, expression.span));
        }
        return call_function(builtins_, *function, std::move(arguments), settings, current_file, session);
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
            ensure_scalar_comparison(left, right, expression.span);
            return Value(left.equals(right));
        }
        if (binary.op == "!=") {
            ensure_scalar_comparison(left, right, expression.span);
            return Value(!left.equals(right));
        }
        if (binary.op == "<" || binary.op == ">" || binary.op == "<=" || binary.op == ">=") {
            ensure_scalar_operand(left, expression.span, binary.op);
            ensure_scalar_operand(right, expression.span, binary.op);
            const auto ordering = left.compare_scalar(right);
            if (!ordering.has_value()) {
                throw DiagnosticError(make_runtime_error("Comparison requires scalar operands", expression.span));
            }
            return Value(compare_with_order(*ordering, binary.op));
        }
        if (binary.op == "in") {
            ensure_scalar_operand(left, expression.span, "in");
            if (!right.is_list() && !right.is_object() && !right.try_as_string_view().has_value()) {
                throw DiagnosticError(make_runtime_error("Operator 'in' requires string, list, or object on right side", expression.span));
            }
            return Value(contains_value(right, left));
        }
        throw DiagnosticError(make_runtime_error("Unsupported binary operator: " + binary.op, expression.span));
    }
    case ExpressionKind::Grouped:
        return evaluate(*static_cast<const GroupedExpr&>(expression).expression, settings, session, current_file);
    case ExpressionKind::FilterCall: {
        const auto& filter = static_cast<const FilterCallExpr&>(expression);
        std::vector<Value> arguments;
        arguments.reserve(filter.arguments.size() + 1);
        arguments.push_back(evaluate(*filter.input, settings, session, current_file));
        for (const auto& argument : filter.arguments) {
            arguments.push_back(evaluate(*argument, settings, session, current_file));
        }
        return filters_.apply(filter.name, arguments);
    }
    }

    throw DiagnosticError(make_runtime_error("Unsupported expression node", expression.span));
}

}
