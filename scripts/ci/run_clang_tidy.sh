#!/usr/bin/env bash
set -euo pipefail

ROOT="${CLANG_TIDY_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
BUILD_DIR="${CLANG_TIDY_BUILD_DIR:-$ROOT/build-cmake/tidy}"
CMAKE_PRESET="${CLANG_TIDY_PRESET:-tidy}"
BUILD_PRESET="${CLANG_TIDY_BUILD_PRESET:-tidy-core}"

ANALYZE_CHECKS='clang-analyzer-*,-clang-analyzer-optin.performance.Padding'
LINT_CHECKS='bugprone-*,-bugprone-easily-swappable-parameters,misc-throw-by-value-catch-by-reference,misc-use-after-move,performance-for-range-copy,performance-implicit-conversion-in-loop,performance-inefficient-vector-operation,performance-move-const-arg,performance-no-int-to-ptr,performance-noexcept-move-constructor,performance-unnecessary-copy-initialization,performance-unnecessary-value-param,readability-container-contains,readability-redundant-control-flow,readability-redundant-string-init,readability-suspicious-call-argument,modernize-avoid-bind,modernize-make-unique,modernize-make-shared,modernize-deprecated-headers,portability-simd-intrinsics'
SECURITY_CHECKS='cert-*,-cert-env33-c,concurrency-*,-concurrency-mt-unsafe,cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-type-cstyle-cast,cppcoreguidelines-pro-bounds-constant-array-index'

resolve_run_clang_tidy() {
    if command -v run-clang-tidy >/dev/null 2>&1; then
        printf '%s\n' run-clang-tidy
        return 0
    fi

    local candidate
    for candidate in run-clang-tidy-21 run-clang-tidy-20 run-clang-tidy-19 run-clang-tidy-18; do
        if command -v "$candidate" >/dev/null 2>&1; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    printf 'run-clang-tidy not found; install clang-tidy\n' >&2
    return 1
}

ensure_build() {
    if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
        cmake --preset "$CMAKE_PRESET" "$@"
    fi

    cmake --build --preset "$BUILD_PRESET" --parallel
}

run_profile() {
    local profile=$1
    local checks=$2
    local runner
    runner=$(resolve_run_clang_tidy)

    printf 'Running clang-tidy profile: %s\n' "$profile"

    local output=""
    local status=0
    output=$("$runner" -p "$BUILD_DIR" -checks="$checks" -quiet 2>&1) || status=$?

    if [[ -n "$output" ]]; then
        printf '%s\n' "$output"
    fi

    if echo "$output" | grep -qE '(^|[^[])-warnings-as-errors\]|clang-diagnostic-error'; then
        printf 'clang-tidy profile "%s" failed\n' "$profile" >&2
        return 1
    fi

    if [[ $status -ne 0 ]]; then
        return "$status"
    fi
}

usage() {
    cat <<EOF
Usage: $(basename "$0") <analyze|lint|security|all> [extra cmake configure args...]

Profiles:
  analyze   Static analyzer checks (clang-analyzer-*)
  lint      Bug-prone, performance, and modernization checks
  security  CERT, concurrency, and bounds checks
  all       Run analyze, lint, and security
EOF
}

main() {
    local profile=${1:-}
    if [[ -z "$profile" ]]; then
        usage >&2
        return 2
    fi
    shift

    ensure_build "$@"

    case "$profile" in
        analyze)
            run_profile analyze "$ANALYZE_CHECKS"
            ;;
        lint)
            run_profile lint "$LINT_CHECKS"
            ;;
        security)
            run_profile security "$SECURITY_CHECKS"
            ;;
        all)
            run_profile analyze "$ANALYZE_CHECKS"
            run_profile lint "$LINT_CHECKS"
            run_profile security "$SECURITY_CHECKS"
            ;;
        *)
            usage >&2
            return 2
            ;;
    esac
}

main "$@"
