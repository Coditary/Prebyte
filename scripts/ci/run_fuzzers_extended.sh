#!/usr/bin/env bash
set -uo pipefail

ROOT="${FUZZ_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
BUILD_DIR="${FUZZ_BUILD_DIR:-$ROOT/build-cmake/fuzz}"
MAX_TOTAL_TIME="${PREBYTE_FUZZ_MAX_TOTAL_TIME:-300}"
LOG="${FUZZ_LOG:-$ROOT/build-cmake/fuzz-run-extended.log}"

FUZZ_TARGETS=(
    fuzz_template_lexer
    fuzz_template_parser
    fuzz_json_parser
    fuzz_yaml_parser
    fuzz_toml_parser
    fuzz_ini_parser
    fuzz_env_parser
    fuzz_compiled_template_serializer
    fuzz_settings_loader
    fuzz_include_resolver
)

seed_dir_for_target() {
    case "$1" in
        fuzz_template_lexer|fuzz_template_parser)
            printf '%s/tests/fuzz/seeds/template\n' "$ROOT"
            ;;
        fuzz_json_parser)
            printf '%s/tests/fuzz/seeds/json\n' "$ROOT"
            ;;
        fuzz_yaml_parser)
            printf '%s/tests/fuzz/seeds/yaml\n' "$ROOT"
            ;;
        fuzz_toml_parser)
            printf '%s/tests/fuzz/seeds/toml\n' "$ROOT"
            ;;
        fuzz_ini_parser)
            printf '%s/tests/fuzz/seeds/ini\n' "$ROOT"
            ;;
        fuzz_env_parser)
            printf '%s/tests/fuzz/seeds/env\n' "$ROOT"
            ;;
        fuzz_compiled_template_serializer)
            printf '%s/tests/fuzz/seeds/pbc\n' "$ROOT"
            ;;
        fuzz_settings_loader)
            printf '%s/tests/fuzz/seeds/settings\n' "$ROOT"
            ;;
        fuzz_include_resolver)
            printf '%s/tests/fuzz/seeds/include\n' "$ROOT"
            ;;
        *)
            return 1
            ;;
    esac
}

validate_seed_uniqueness() {
    local seeds_dir=$1
    local -A seen_hashes=()
    local seed_file hash duplicate

    shopt -s nullglob
    for seed_file in "$seeds_dir"/*; do
        [[ -f "$seed_file" ]] || continue
        hash=$(sha1sum "$seed_file" | awk '{print $1}')
        duplicate="${seen_hashes[$hash]:-}"
        if [[ -n "$duplicate" ]]; then
            printf 'Duplicate fuzz seed content:\n  %s\n  %s\n' "$duplicate" "$seed_file" >&2
            return 1
        fi
        seen_hashes[$hash]=$seed_file
    done
}

corpus_contains_seed() {
    local corpus=$1
    local seed_file=$2
    local hash entry

    hash=$(sha1sum "$seed_file" | awk '{print $1}')
    if [[ -f "$corpus/$hash" ]]; then
        return 0
    fi

    shopt -s nullglob
    for entry in "$corpus"/*; do
        [[ -f "$entry" ]] || continue
        if cmp -s "$seed_file" "$entry"; then
            return 0
        fi
    done

    return 1
}

install_seed_into_corpus() {
    local seed_file=$1
    local corpus=$2
    local hash

    if corpus_contains_seed "$corpus" "$seed_file"; then
        return 0
    fi

    hash=$(sha1sum "$seed_file" | awk '{print $1}')
    cp "$seed_file" "$corpus/$hash"
}

bootstrap_corpus() {
    local target=$1
    local corpus="$ROOT/tests/fuzz/corpus/$target"
    local seeds_dir
    seeds_dir=$(seed_dir_for_target "$target")

    mkdir -p "$corpus"
    validate_seed_uniqueness "$seeds_dir"

    shopt -s nullglob
    for seed_file in "$seeds_dir"/*; do
        [[ -f "$seed_file" ]] || continue
        install_seed_into_corpus "$seed_file" "$corpus"
    done
}

export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"

: >"$LOG"
failures=0

for target in "${FUZZ_TARGETS[@]}"; do
    bootstrap_corpus "$target"
    corpus="$ROOT/tests/fuzz/corpus/$target"
    {
        printf '\n========== %s (%ss) ==========\n' "$target" "$MAX_TOTAL_TIME"
        "$BUILD_DIR/$target" \
            -max_total_time="$MAX_TOTAL_TIME" \
            -close_fd_mask=3 \
            "$corpus"
        status=$?
        if [[ $status -ne 0 ]]; then
            printf 'FAILED: %s (exit %s)\n' "$target" "$status"
            failures=$((failures + 1))
        else
            printf 'PASSED: %s\n' "$target"
        fi
    } 2>&1 | tee -a "$LOG"
done

printf '\nSummary: %s/%s fuzzers failed\n' "$failures" "${#FUZZ_TARGETS[@]}" | tee -a "$LOG"
exit "$failures"
