# Fault tolerance

Stress and regression coverage for malformed input, parser limits, and runtime edge cases.

- `fuzz/` — libFuzzer targets, curated seeds, and generated corpus (`corpus/` is gitignored).
- `regression/` — deterministic tests for previously found fuzzer crashes and limit violations.
- `sanitizers/` — documentation for ASan/UBSan, TSan, and MSan runs (see README there).

Run fuzzers: `make fuzz` or `scripts/ci/run_fuzzers.sh`.

Run sanitizers: `make sanitize`, `make tsan`, `make msan` (re-execute the full `prebyte_tests` binary under instrumentation).
