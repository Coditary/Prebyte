#!/usr/bin/env bash
set -euo pipefail

ROOT="${COVERAGE_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
OBJECT_DIR="${COVERAGE_OBJECT_DIR:-$ROOT/build-cmake/coverage}"
OUTPUT="${COVERAGE_OUTPUT:-$OBJECT_DIR/coverage.xml}"
FILTER="${COVERAGE_FILTER:-$ROOT/src/main/cpp}"
MIN_LINE="${COVERAGE_MIN_LINE:-85}"

gcovr \
  --root "$ROOT" \
  --object-directory "$OBJECT_DIR" \
  --filter "$FILTER" \
  --gcov-ignore-parse-errors=negative_hits.warn \
  --fail-under-line "$MIN_LINE" \
  --xml-pretty \
  --output "$OUTPUT"
