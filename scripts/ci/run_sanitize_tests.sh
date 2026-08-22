#!/usr/bin/env bash
set -euo pipefail

ROOT="${SANITIZE_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"

usage() {
    cat <<EOF
Usage: $(basename "$0") <asan|tsan|msan> [cmake configure args...]

Runs the test suite with the selected Clang sanitizer preset.
EOF
}

SANITIZER_KIND="${1:-}"
if [[ -z "$SANITIZER_KIND" ]]; then
    usage >&2
    exit 2
fi
shift

case "$SANITIZER_KIND" in
    asan)
        BUILD_DIR="${SANITIZE_BUILD_DIR:-$ROOT/build-cmake/sanitize}"
        CMAKE_PRESET="${SANITIZE_PRESET:-sanitize}"
        BUILD_PRESET="${SANITIZE_BUILD_PRESET:-sanitize-tests}"
        TEST_PRESET="${SANITIZE_TEST_PRESET:-sanitize}"
        unset TSAN_OPTIONS MSAN_OPTIONS
        export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1}"
        export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"
        ;;
    tsan)
        BUILD_DIR="${SANITIZE_BUILD_DIR:-$ROOT/build-cmake/tsan}"
        CMAKE_PRESET="${SANITIZE_PRESET:-tsan}"
        BUILD_PRESET="${SANITIZE_BUILD_PRESET:-tsan-tests}"
        TEST_PRESET="${SANITIZE_TEST_PRESET:-tsan}"
        unset ASAN_OPTIONS UBSAN_OPTIONS MSAN_OPTIONS
        export TSAN_OPTIONS="${TSAN_OPTIONS:-halt_on_error=1:history_size=7:second_deadlock_stack=1}"
        ;;
    msan)
        BUILD_DIR="${SANITIZE_BUILD_DIR:-$ROOT/build-cmake/msan}"
        CMAKE_PRESET="${SANITIZE_PRESET:-msan}"
        BUILD_PRESET="${SANITIZE_BUILD_PRESET:-msan-tests}"
        TEST_PRESET="${SANITIZE_TEST_PRESET:-msan}"
        unset ASAN_OPTIONS UBSAN_OPTIONS TSAN_OPTIONS
        export MSAN_OPTIONS="${MSAN_OPTIONS:-halt_on_error=1:print_stats=1}"
        export PREBYTE_MSAN_LIBCXX_PREFIX="${PREBYTE_MSAN_LIBCXX_PREFIX:-${XDG_CACHE_HOME:-$HOME/.cache}/prebyte-msan-libcxx}"
        chmod +x "$ROOT/scripts/ci/bootstrap_msan_libcxx.sh"
        "$ROOT/scripts/ci/bootstrap_msan_libcxx.sh"
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

CMAKE_EXTRA_ARGS=("$@")
if [[ "$SANITIZER_KIND" == "msan" ]]; then
    CMAKE_EXTRA_ARGS=(-DPREBYTE_MSAN_LIBCXX_PREFIX="$PREBYTE_MSAN_LIBCXX_PREFIX" "$@")
fi

if [[ "$SANITIZER_KIND" == "msan" ]] || [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake --preset "$CMAKE_PRESET" "${CMAKE_EXTRA_ARGS[@]}"
fi

cmake --build --preset "$BUILD_PRESET" --parallel
ctest --preset "$TEST_PRESET" --output-on-failure
