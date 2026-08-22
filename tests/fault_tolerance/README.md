# Fault tolerance

Stress and regression coverage for malformed input, parser limits, and runtime edge cases.

- `fuzz/` — libFuzzer targets, curated seeds, generated corpus (`corpus/` is gitignored), and `fuzz/regression/` replay inputs.
- `regression/` — deterministic tests for previously found fuzzer crashes and limit violations.
- `sanitizers/` — documentation for ASan/UBSan, TSan, and MSan runs (see README there).

Run fuzzers: `make fuzz` or `scripts/ci/run_fuzzers.sh` (includes regression replay at the end).

Import a new crash input:

```bash
python3 scripts/ci/import_fuzz_regression.py --target fuzz_template_lexer --crash /path/to/crash
```

Replay regression corpus only: `make fuzz-regression` or `scripts/ci/run_fuzz_regression.sh`.

Run sanitizers: `make sanitize`, `make tsan`, `make msan` (re-execute the full `prebyte_tests` binary under instrumentation).
