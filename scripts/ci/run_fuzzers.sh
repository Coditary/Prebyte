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
    fuzz_yaml_parser
    fuzz_toml_parser
    fuzz_ini_parser
    fuzz_env_parser
    fuzz_compiled_template_serializer
    fuzz_settings_loader
    fuzz_include_resolver
    fuzz_file_parser
    fuzz_lua_chunk
    fuzz_render_pbt
    fuzz_app_runner
    fuzz_batch_render
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
        fuzz_file_parser)
            printf '%s/tests/fuzz/seeds/file_parser\n' "$ROOT"
            ;;
        fuzz_lua_chunk)
            printf '%s/tests/fuzz/seeds/lua_chunk\n' "$ROOT"
            ;;
        fuzz_render_pbt)
            printf '%s/tests/fuzz/seeds/render_pbt\n' "$ROOT"
            ;;
        fuzz_app_runner)
            printf '%s/tests/fuzz/seeds/app_runner\n' "$ROOT"
            ;;
        fuzz_batch_render)
            printf '%s/tests/fuzz/seeds/batch_render\n' "$ROOT"
            ;;
        *)
            printf 'Unknown fuzz target: %s\n' "$1" >&2
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
    local hash basename target

    if corpus_contains_seed "$corpus" "$seed_file"; then
        return 0
    fi

    hash=$(sha1sum "$seed_file" | awk '{print $1}')
    basename=$(basename "$seed_file")
    if [[ "$basename" =~ ^[0-9a-f]{40}$ ]]; then
        target=$(python3 "$ROOT/scripts/ci/name_fuzz_corpus_entry.py" --print-name "$seed_file")
    else
        target="$basename"
    fi

    if [[ -f "$corpus/$target" ]] && ! cmp -s "$seed_file" "$corpus/$target"; then
        target="${target}_${hash:0:8}"
    fi

    cp "$seed_file" "$corpus/$target"
}

# shellcheck source=scripts/ci/fuzz_seed_guard.sh
source "$ROOT/scripts/ci/fuzz_seed_guard.sh"

bootstrap_corpus() {
    local target=$1
    local corpus="$ROOT/tests/fuzz/corpus/$target"
    local seeds_dir
    seeds_dir=$(seed_dir_for_target "$target")

    mkdir -p "$corpus"
    relocate_generated_seed_artifacts "$seeds_dir" "$corpus"
    validate_seed_uniqueness "$seeds_dir"

    shopt -s nullglob
    for seed_file in "$seeds_dir"/*; do
        [[ -f "$seed_file" ]] || continue
        install_seed_into_corpus "$seed_file" "$corpus"
    done

    rename_fuzz_corpus_entries "$corpus"
}

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake --preset "$CMAKE_PRESET" "$@"
fi

cmake --build --preset "$BUILD_PRESET" --parallel

export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"

for target in "${FUZZ_TARGETS[@]}"; do
    bootstrap_corpus "$target"
    corpus="$ROOT/tests/fuzz/corpus/$target"
    printf 'Running fuzzer %s for %ss\n' "$target" "$MAX_TOTAL_TIME"
    "$BUILD_DIR/$target" \
        -max_total_time="$MAX_TOTAL_TIME" \
        -close_fd_mask=3 \
        "$corpus"
    rename_fuzz_corpus_entries "$corpus"
done
