#pragma once

#include "config/RuleResolver.h"
#include "runtime/BuiltinRegistry.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/IncludeResolver.h"
#include "runtime/Renderer.h"

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
