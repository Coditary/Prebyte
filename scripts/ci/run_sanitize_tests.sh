#!/usr/bin/env bash
set -euo pipefail

ROOT="${SANITIZE_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
BUILD_DIR="${SANITIZE_BUILD_DIR:-$ROOT/build-cmake/sanitize}"
CMAKE_PRESET="${SANITIZE_PRESET:-sanitize}"
BUILD_PRESET="${SANITIZE_BUILD_PRESET:-sanitize-tests}"
TEST_PRESET="${SANITIZE_TEST_PRESET:-sanitize}"

export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake --preset "$CMAKE_PRESET" "$@"
fi

cmake --build --preset "$BUILD_PRESET" --parallel
ctest --preset "$TEST_PRESET" --output-on-failure
