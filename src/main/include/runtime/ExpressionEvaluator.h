#pragma once

#include <filesystem>

#include "config/ConfigTypes.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/ExpressionEngine.h"
#include "runtime/RenderSession.h"
#include "runtime/Value.h"
#include "template/ast/Expression.h"

namespace prebyte {

class ExpressionEvaluator : public ExpressionEngine {
public:
    explicit ExpressionEvaluator(const BuiltinRegistry& builtins);

    Value evaluate(const ExpressionNode& expression, const EffectiveSettings& settings,
                   const RenderSession& session, const std::filesystem::path& current_file) const override;

private:
    Value evaluate_identifier(const IdentifierExpr& expression, const EffectiveSettings& settings,
                              const RenderSession& session, const std::filesystem::path& current_file) const;
    std::string normalize_string(std::string value, const EffectiveSettings& settings) const;

    const BuiltinRegistry& builtins_;
};

}
