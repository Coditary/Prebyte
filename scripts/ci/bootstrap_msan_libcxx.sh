#!/usr/bin/env bash
set -euo pipefail

INSTALL_PREFIX="${PREBYTE_MSAN_LIBCXX_PREFIX:-${XDG_CACHE_HOME:-$HOME/.cache}/prebyte-msan-libcxx}"
LLVM_TAG="${PREBYTE_MSAN_LIBCXX_LLVM_TAG:-llvmorg-18.1.8}"
MARKER_FILE="$INSTALL_PREFIX/.bootstrap-complete"
WORK_DIR="${PREBYTE_MSAN_LIBCXX_WORK_DIR:-${TMPDIR:-/tmp}/prebyte-msan-libcxx-build}"
# Bump this whenever the build configuration changes so stale prefixes are rebuilt.
CONFIG_STAMP="tag=$LLVM_TAG runtimes=libcxx,libcxxabi unwinder=libgcc_s v2"

usage() {
    cat <<EOF
Usage: $(basename "$0")

Builds a MemorySanitizer-instrumented libc++/libc++abi install for local and CI MSan runs.

Environment:
  PREBYTE_MSAN_LIBCXX_PREFIX   Install destination (default: ~/.cache/prebyte-msan-libcxx)
  PREBYTE_MSAN_LIBCXX_LLVM_TAG LLVM release tag to build (default: llvmorg-18.1.8)
  PREBYTE_MSAN_LIBCXX_WORK_DIR Temporary build directory
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ -f "$MARKER_FILE" ]] && grep -qxF "$CONFIG_STAMP" "$MARKER_FILE"; then
    printf 'MSan libc++ already installed at %s\n' "$INSTALL_PREFIX"
    exit 0
fi

for tool in clang clang++ cmake ninja git; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Missing required tool for MSan libc++ bootstrap: %s\n' "$tool" >&2
        exit 1
    fi
done

# Safety: only wipe directories that look like a dedicated MSan libc++ prefix.
if [[ -d "$INSTALL_PREFIX" && "$INSTALL_PREFIX" != *prebyte-msan-libcxx* && ! -f "$MARKER_FILE" ]]; then
    printf 'Refusing to wipe unfamiliar PREBYTE_MSAN_LIBCXX_PREFIX: %s\n' "$INSTALL_PREFIX" >&2
    exit 1
fi
rm -rf "$INSTALL_PREFIX"
mkdir -p "$INSTALL_PREFIX" "$WORK_DIR"
LLVM_SRC="$WORK_DIR/llvm-project"

if [[ ! -d "$LLVM_SRC/.git" ]]; then
    git clone --depth 1 --branch "$LLVM_TAG" https://github.com/llvm/llvm-project.git "$LLVM_SRC"
fi

BUILD_DIR="$WORK_DIR/build"
rm -rf "$BUILD_DIR"
# NOTE: libunwind must not be MSan-instrumented. Its register context is
# filled by assembly (__unw_getcontext), which MSan cannot see, so every C++
# exception throw is reported as use-of-uninitialized-value inside
# UnwindCursor::getReg; the report path then unwinds again and recurses until
# stack overflow (https://github.com/llvm/llvm-project/issues/84348).
# libc++abi therefore falls back to the system libgcc_s unwinder here.
cmake -G Ninja -S "$LLVM_SRC/runtimes" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
    -DLLVM_USE_SANITIZER=Memory \
    -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
    -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DLIBCXX_INSTALL_MODULES=OFF

cmake --build "$BUILD_DIR" --target install-cxx install-cxxabi --parallel
{ date -u +%Y-%m-%dT%H:%M:%SZ; printf '%s\n' "$CONFIG_STAMP"; } >"$MARKER_FILE"
printf 'Installed MSan libc++ to %s\n' "$INSTALL_PREFIX"
