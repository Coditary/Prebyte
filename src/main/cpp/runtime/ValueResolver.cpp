#include "runtime/ValueResolver.h"

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

std::optional<std::size_t> parse_index_value(const Value& index) {
    const std::string text = index.to_string();
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::stoull(text));
}

std::optional<std::string> resolve_render_arg(const std::string& name, const SourceSpan& span,
                                              const RenderSession& session) {
    const auto& args = session.args_view();
    if (!text::starts_with(name, "ARGS")) {
        return std::nullopt;
    }

    if (name == "ARGS") {
        throw DiagnosticError(make_runtime_error("ARGS must be accessed as ARGS[index]", span));
    }
    if (!text::starts_with(name, "ARGS[") || !text::ends_with(name, "]")) {
        throw DiagnosticError(make_runtime_error("Invalid ARGS reference: " + name + ". Use ARGS[index]", span));
    }

    const std::string_view index_text(name.data() + 5, name.size() - 6);
    if (index_text.empty() || !std::all_of(index_text.begin(), index_text.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        })) {
        throw DiagnosticError(make_runtime_error("Invalid ARGS index in " + name, span));
    }

    const std::size_t index = static_cast<std::size_t>(std::stoull(std::string(index_text)));
    if (index >= args.size()) {
        throw DiagnosticError(make_runtime_error("ARGS index out of range: " + name, span));
    }

    return args[index];
}

}

ValueResolver::ValueResolver(const BuiltinRegistry& builtins)
    : builtins_(builtins) {}

Value ValueResolver::resolve_identifier(const std::string& name, const SourceSpan& span,
                                        const EffectiveSettings& settings, const RenderSession& session,
                                        const std::filesystem::path& current_file) const {
    if (const Value* scoped_value = session.lookup_scoped_value(name, settings.case_sensitive_variables)) {
        return normalize_value(*scoped_value, settings);
    }

    if (session.ignore_names_view().contains(name)) {
        return Value(std::string());
    }

    if (const auto value = lookup_direct_identifier(name, span, settings, session, current_file)) {
        return normalize_value(*value, settings);
    }

    if (name.find('.') != std::string::npos && !text::starts_with(name, "ARGS")) {
        return resolve_member_path(name, span, settings, session, current_file);
    }

    return resolve_missing_identifier(name, span, settings);
}

Value ValueResolver::resolve_member(const Value& base, std::string_view member, const SourceSpan& span,
                                    const EffectiveSettings& settings) const {
    if (base.is_null()) {
        return resolve_missing_path(member, span, settings);
    }

    if (const auto value = base.member(member)) {
        return normalize_value(*value, settings);
    }

    if (base.is_object()) {
        return resolve_missing_path(member, span, settings);
    }

    throw DiagnosticError(make_runtime_error("Cannot access member '" + std::string(member)
                                             + "' on non-object value",
                                             span));
}

Value ValueResolver::resolve_index(const Value& base, const Value& index, const SourceSpan& span,
                                   const EffectiveSettings& settings) const {
    if (base.is_null()) {
        return resolve_missing_path("[index]", span, settings);
    }

    if (base.is_list()) {
        const auto parsed = parse_index_value(index);
        if (!parsed.has_value()) {
            throw DiagnosticError(make_runtime_error("List index must be non-negative integer", span));
        }
        if (const auto value = base.index(*parsed)) {
            return normalize_value(*value, settings);
        }
        return resolve_missing_path("[index]", span, settings);
    }

    if (base.is_object()) {
        if (index.is_object() || index.is_list()) {
            throw DiagnosticError(make_runtime_error("Object key must be scalar value", span));
        }
        if (const auto value = base.member(index.to_string())) {
            return normalize_value(*value, settings);
        }
        return resolve_missing_path(index.to_string(), span, settings);
    }

    throw DiagnosticError(make_runtime_error("Cannot index non-container value", span));
}

Value ValueResolver::resolve_member_path(std::string_view name, const SourceSpan& span,
                                         const EffectiveSettings& settings, const RenderSession& session,
                                         const std::filesystem::path& current_file) const {
    const std::size_t first_dot = name.find('.');
    if (first_dot == std::string_view::npos || first_dot == 0 || first_dot + 1 >= name.size()) {
        throw DiagnosticError(make_runtime_error("Invalid member access: " + std::string(name), span));
    }

    const std::string_view root_name = name.substr(0, first_dot);
    const auto root = lookup_direct_identifier(root_name, span, settings, session, current_file);
    if (!root.has_value()) {
        return resolve_missing_identifier(name, span, settings);
    }

    Value current = normalize_value(*root, settings);
    std::size_t segment_start = first_dot + 1;
    while (segment_start < name.size()) {
        const std::size_t next_dot = name.find('.', segment_start);
        const std::string_view segment = next_dot == std::string_view::npos
            ? name.substr(segment_start)
            : name.substr(segment_start, next_dot - segment_start);
        if (segment.empty()) {
            throw DiagnosticError(make_runtime_error("Invalid member access: " + std::string(name), span));
        }

        const auto member = current.member(segment);
        if (!member.has_value()) {
            if (current.is_null()) {
                return resolve_missing_identifier(name, span, settings);
            }
            if (current.is_object()) {
                return resolve_missing_identifier(name, span, settings);
            }
            throw DiagnosticError(make_runtime_error("Cannot access member '" + std::string(segment)
                                                     + "' on non-object value",
                                                     span));
        }

        current = normalize_value(*member, settings);
        if (next_dot == std::string_view::npos) {
            return current;
        }
        segment_start = next_dot + 1;
    }

    return current;
}

std::optional<Value> ValueResolver::lookup_direct_identifier(std::string_view name, const SourceSpan& span,
                                                             const EffectiveSettings& settings,
                                                             const RenderSession& session,
                                                             const std::filesystem::path& current_file) const {
    if (const Value* scoped_value = session.lookup_scoped_value(name, settings.case_sensitive_variables)) {
        return *scoped_value;
    }

    if (const auto render_arg = resolve_render_arg(std::string(name), span, session)) {
        return Value(*render_arg);
    }

    if (const auto builtin_value = builtins_.resolve(std::string(name), span, current_file, session)) {
        return Value(*builtin_value);
    }

    if (const Value* variable = session.variables_view().get_value(name, settings.case_sensitive_variables)) {
        return *variable;
    }

    if (settings.allow_env) {
        if (settings.forbidden_env_vars.contains(std::string(name))) {
            throw DiagnosticError(make_runtime_error("Access to forbidden environment variable: " + std::string(name), span));
        }
        if (const char* env = std::getenv(std::string(name).c_str())) {
            return Value(std::string(env));
        }
    }

    return std::nullopt;
}

Value ValueResolver::resolve_missing_identifier(std::string_view name, const SourceSpan& span,
                                                const EffectiveSettings& settings) const {
    return resolve_missing_path(name, span, settings);
}

Value ValueResolver::resolve_missing_path(std::string_view path, const SourceSpan& span,
                                         const EffectiveSettings& settings) const {
    if (!settings.strict_variables && settings.default_variable_value.has_value()) {
        return Value(normalize_string(*settings.default_variable_value, settings));
    }

    if (!settings.strict_variables) {
        return Value();
    }

    throw DiagnosticError(make_runtime_error("Unknown variable: " + std::string(path), span));
}

Value ValueResolver::normalize_value(Value value, const EffectiveSettings& settings) const {
    if (const auto string_value = value.try_as_string_view()) {
        return Value(normalize_string(std::string(*string_value), settings));
    }
    return value;
}

std::string ValueResolver::normalize_string(std::string value, const EffectiveSettings& settings) const {
    if (settings.trim) {
        value = text::trim(std::move(value));
    }
    if (settings.has_max_variable_length && value.size() > settings.max_variable_length) {
        value.resize(settings.max_variable_length);
    }
    return value;
}

}
