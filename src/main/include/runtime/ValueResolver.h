#pragma once

#include <filesystem>
#include <string>

#include "config/ConfigTypes.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/RenderSession.h"
#include "runtime/Value.h"
#include "support/SourceSpan.h"

namespace prebyte {

class ValueResolver {
public:
    explicit ValueResolver(const BuiltinRegistry& builtins);

    Value resolve_identifier(const std::string& name, const SourceSpan& span, const EffectiveSettings& settings,
                             const RenderSession& session, const std::filesystem::path& current_file) const;
    Value resolve_member(const Value& base, std::string_view member, const SourceSpan& span,
                         const EffectiveSettings& settings) const;
    Value resolve_index(const Value& base, const Value& index, const SourceSpan& span,
                        const EffectiveSettings& settings) const;
    std::string normalize_string(std::string value, const EffectiveSettings& settings) const;

private:
    Value resolve_member_path(std::string_view name, const SourceSpan& span, const EffectiveSettings& settings,
                              const RenderSession& session, const std::filesystem::path& current_file) const;
    std::optional<Value> lookup_direct_identifier(std::string_view name, const SourceSpan& span,
                                                  const EffectiveSettings& settings, const RenderSession& session,
                                                  const std::filesystem::path& current_file) const;
    Value resolve_missing_identifier(std::string_view name, const SourceSpan& span,
                                     const EffectiveSettings& settings) const;
    Value normalize_value(Value value, const EffectiveSettings& settings) const;
    Value resolve_missing_path(std::string_view path, const SourceSpan& span,
                               const EffectiveSettings& settings) const;

    const BuiltinRegistry& builtins_;
};

}
