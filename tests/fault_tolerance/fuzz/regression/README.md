# Fuzzer regression corpus

Deterministic crash inputs replayed on every fuzz-regression run. Import new crashes with:

```bash
python3 scripts/ci/import_fuzz_regression.py \
  --target fuzz_template_lexer \
  --crash /path/to/crash-input
```

Replay all entries:

```bash
./scripts/ci/run_fuzz_regression.sh
```

Or via `make fuzz-regression` after a fuzz build.
