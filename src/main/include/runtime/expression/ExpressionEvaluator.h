#pragma once

#include <filesystem>

#include "config/ConfigTypes.h"
#include "runtime/expression/BuiltinRegistry.h"
#include "runtime/expression/ExpressionEngine.h"
#include "runtime/expression/FilterRegistry.h"
#include "runtime/core/RenderSession.h"
#include "runtime/core/Value.h"
#include "runtime/expression/ValueResolver.h"
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
