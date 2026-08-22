#!/usr/bin/env bash

# Relocate libFuzzer-generated SHA1 corpus entries that were accidentally written
# into a curated seed directory back into the target corpus directory.

name_fuzz_corpus_entry() {
    python3 "$ROOT/scripts/ci/name_fuzz_corpus_entry.py" "$1"
}

rename_fuzz_corpus_entries() {
    local corpus=$1
    python3 "$ROOT/scripts/ci/name_fuzz_corpus_entry.py" --rename-dir "$corpus"
}

relocate_generated_seed_artifacts() {
    local seeds_dir=$1
    local corpus=$2
    local artifact

    while IFS= read -r -d '' artifact; do
        install_seed_into_corpus "$artifact" "$corpus"
        rm -f "$artifact"
    done < <(find "$seeds_dir" -maxdepth 1 -type f -regextype posix-extended -regex '.*/[0-9a-f]{40}$' -print0 2>/dev/null)

    rename_fuzz_corpus_entries "$corpus"
}
