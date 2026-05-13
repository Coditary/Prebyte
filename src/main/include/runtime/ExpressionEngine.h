#pragma once

#include <filesystem>

#include "config/ConfigTypes.h"
#include "runtime/RenderSession.h"
#include "runtime/Value.h"
#include "template/ast/Expression.h"

namespace prebyte {

class ExpressionEngine {
public:
    virtual ~ExpressionEngine() = default;

    virtual Value evaluate(const ExpressionNode& expression, const EffectiveSettings& settings,
                           const RenderSession& session, const std::filesystem::path& current_file) const = 0;
};

}
