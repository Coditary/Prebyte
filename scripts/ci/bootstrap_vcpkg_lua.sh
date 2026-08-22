#!/usr/bin/env bash
set -euo pipefail

ROOT="${FUZZ_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
VCPKG_TRIPLET="${VCPKG_TARGET_TRIPLET:-x64-linux-msan}"
SANITIZER_KIND="${1:-asan}"
shift || true

VCPKG_ROOT="${VCPKG_ROOT:-${RUNNER_TEMP:-/tmp}/vcpkg}"
OVERLAY_TRIPLETS="${VCPKG_OVERLAY_TRIPLETS:-$ROOT/cmake/vcpkg/triplets}"

if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
    git clone --depth 1 https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
    "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
fi

case "$SANITIZER_KIND" in
    msan)
        export CC=clang
        export CXX=clang++
        VCPKG_TRIPLET=x64-linux-msan
        ;;
    asan|tsan)
        VCPKG_TRIPLET="${VCPKG_TARGET_TRIPLET:-x64-linux}"
        ;;
    *)
        printf 'Unsupported sanitizer kind for vcpkg bootstrap: %s\n' "$SANITIZER_KIND" >&2
        exit 1
        ;;
esac

"$VCPKG_ROOT/vcpkg" install "lua:${VCPKG_TRIPLET}" \
    --overlay-triplets="$OVERLAY_TRIPLETS" \
    --binarysource=clear
