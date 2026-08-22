#pragma once

#include "config/RuleResolver.h"
#include "runtime/expression/BuiltinRegistry.h"
#include "runtime/expression/ExpressionEvaluator.h"
#include "runtime/resolution/IncludeResolver.h"
#include "runtime/render/Renderer.h"

namespace prebyte {

struct EngineRuntime {
    EngineRuntime()
        : expression_evaluator(builtins),
          renderer(rule_resolver, include_resolver, expression_evaluator) {}

    RuleResolver rule_resolver;
    BuiltinRegistry builtins;
    ExpressionEvaluator expression_evaluator;
    IncludeResolver include_resolver;
    Renderer renderer;
};

}
