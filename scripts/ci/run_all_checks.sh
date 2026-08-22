#!/usr/bin/env bash
set -euo pipefail

ROOT="${ALL_CHECKS_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$ROOT"

export PREBYTE_PBT_ITERATIONS="${PREBYTE_PBT_ITERATIONS:-250}"

run_step() {
    printf '\n========== %s ==========\n' "$1"
    shift
    "$@"
}

run_step "Coverage (tests + 85%% gate)" make coverage
if [[ "${PREBYTE_SKIP_STATIC_ANALYSIS:-}" != "1" ]]; then
    run_step "Static analysis (clang-tidy analyze, lint, security)" make static-analysis
else
    printf '\nSkipping static analysis (PREBYTE_SKIP_STATIC_ANALYSIS=1).\n'
fi
run_step "Packaging smoke (binary + ReqPack + index)" make packaging-smoke
run_step "Benchmark regression gate" make benchmark-gate
run_step "ASan/UBSan tests" make sanitize
run_step "ThreadSanitizer tests" make tsan
if [[ "${PREBYTE_SKIP_FUZZ:-}" != "1" ]]; then
    run_step "libFuzzer targets + regression replay" make fuzz
else
    printf '\nSkipping libFuzzer targets (PREBYTE_SKIP_FUZZ=1).\n'
fi

if command -v docker >/dev/null 2>&1 && [[ "${PREBYTE_SKIP_DOCKER:-}" != "1" ]]; then
    run_step "Docker packaging smoke" make packaging-smoke-docker
elif [[ "${PREBYTE_SKIP_DOCKER:-}" == "1" ]]; then
    printf '\nSkipping Docker packaging smoke (PREBYTE_SKIP_DOCKER=1).\n'
else
    printf '\nSkipping Docker packaging smoke (docker not installed).\n'
fi

if [[ "${PREBYTE_ALL_CHECKS_MSAN:-}" == "1" ]]; then
    run_step "MemorySanitizer tests" make msan
fi

if [[ "${PREBYTE_ALL_CHECKS_COMPARE_BENCHMARK:-}" == "1" ]]; then
    run_step "Cross-engine benchmark comparison" make compare-benchmark
fi

printf '\n=== All checks passed ===\n'
