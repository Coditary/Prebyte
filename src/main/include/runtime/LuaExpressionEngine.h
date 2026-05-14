#pragma once

#include "runtime/ExpressionEngine.h"
#include "runtime/LuaRuntime.h"

namespace prebyte {

class LuaExpressionEngine : public ExpressionEngine {
public:
    Value evaluate(const ExpressionNode& expression, const EffectiveSettings& settings,
                   RenderSession& session, const std::filesystem::path& current_file) const override;
};

}
