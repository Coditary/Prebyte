#!/usr/bin/env bash
set -euo pipefail

ROOT="${FUZZ_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
BUILD_DIR="${FUZZ_BUILD_DIR:-$ROOT/build-cmake/fuzz}"
CMAKE_PRESET="${FUZZ_PRESET:-fuzz}"
BUILD_PRESET="${FUZZ_BUILD_PRESET:-fuzz}"
MAX_TOTAL_TIME="${PREBYTE_FUZZ_MAX_TOTAL_TIME:-60}"

FUZZ_TARGETS=(
    fuzz_template_lexer
    fuzz_template_parser
    fuzz_json_parser
)

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake --preset "$CMAKE_PRESET" "$@"
fi

cmake --build --preset "$BUILD_PRESET" --parallel

export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"

bootstrap_corpus() {
    local target=$1
    local corpus="$ROOT/tests/fuzz/corpus/$target"
    local seeds="$ROOT/tests/fuzz/seeds/$target"

    mkdir -p "$corpus"
    if [[ -d "$seeds" ]]; then
        cp -n "$seeds/"* "$corpus/" 2>/dev/null || true
    fi
}

for target in "${FUZZ_TARGETS[@]}"; do
    bootstrap_corpus "$target"
    corpus="$ROOT/tests/fuzz/corpus/$target"
    printf 'Running fuzzer %s for %ss\n' "$target" "$MAX_TOTAL_TIME"
    "$BUILD_DIR/$target" \
        -max_total_time="$MAX_TOTAL_TIME" \
        -close_fd_mask=3 \
        "$corpus"
done
