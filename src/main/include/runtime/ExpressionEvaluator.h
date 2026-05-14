#pragma once

#include <filesystem>

#include "config/ConfigTypes.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/ExpressionEngine.h"
#include "runtime/FilterRegistry.h"
#include "runtime/RenderSession.h"
#include "runtime/Value.h"
#include "runtime/ValueResolver.h"
#include "template/ast/Expression.h"

namespace prebyte {

class ExpressionEvaluator : public ExpressionEngine {
public:
    explicit ExpressionEvaluator(const BuiltinRegistry& builtins);
    const BuiltinRegistry& builtins() const;

    Value evaluate(const ExpressionNode& expression, const EffectiveSettings& settings,
                   RenderSession& session, const std::filesystem::path& current_file) const override;

private:
    const BuiltinRegistry& builtins_;
    FilterRegistry filters_;
    ValueResolver resolver_;
};

}
