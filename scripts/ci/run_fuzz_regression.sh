#!/usr/bin/env bash
set -euo pipefail

ROOT="${FUZZ_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
BUILD_DIR="${FUZZ_BUILD_DIR:-$ROOT/build-cmake/fuzz}"
REGRESSION_DIR="$ROOT/tests/fault_tolerance/fuzz/regression"

if [[ ! -d "$REGRESSION_DIR" ]]; then
    printf 'Regression directory not found: %s\n' "$REGRESSION_DIR" >&2
    exit 1
fi

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    printf 'Fuzz build not found at %s. Configure with cmake --preset fuzz first.\n' "$BUILD_DIR" >&2
    exit 1
fi

replayed=0
for target_dir in "$REGRESSION_DIR"/*/; do
    [[ -d "$target_dir" ]] || continue
    target=$(basename "$target_dir")
    fuzzer="$BUILD_DIR/$target"
    if [[ ! -x "$fuzzer" ]]; then
        printf 'Missing fuzzer binary: %s\n' "$fuzzer" >&2
        exit 1
    fi

    shopt -s nullglob
    entries=("$target_dir"/*)
    if [[ ${#entries[@]} -eq 0 ]]; then
        continue
    fi

    for input in "${entries[@]}"; do
        [[ -f "$input" ]] || continue
        printf 'Replaying %s <= %s\n' "$target" "$(basename "$input")"
        "$fuzzer" "$input" -runs=1
        replayed=$((replayed + 1))
    done
done

if [[ "$replayed" -eq 0 ]]; then
    printf 'No regression inputs found under %s\n' "$REGRESSION_DIR" >&2
    exit 1
fi

printf 'Replayed %s regression input(s) successfully.\n' "$replayed"
