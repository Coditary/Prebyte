# Tests

Tests are grouped by non-functional requirement (NFR). Shared assets live in `fixtures/`, `harness/`, and `support/`.

| NFR | Directory | What runs here |
| --- | --- | --- |
| Correctness | `correctness/` | Unit, integration, and property tests (`prebyte_tests`) |
| Fault tolerance | `fault_tolerance/` | Fuzzers, fuzz regressions, sanitizer guidance |
| Security | `security/` | Sandbox and hardening end-to-end tests |
| Concurrency | `concurrency/` | Parallel render and cache stability tests |
| Portability | `portability/` | CLI subprocess tests and packaging smoke tests |
| Performance | `performance/` | Benchmark driver and timing history |

## Running

```bash
cmake --build --preset coverage --target prebyte_tests prebyte
ctest --preset coverage
make coverage      # correctness + coverage gate
make test          # fast dev test run
make               # full local validation (same as make all)
make all           # full local validation
make start         # build CLI only
make sanitize      # ASan/UBSan over the full test binary
make tsan          # ThreadSanitizer
make msan          # MemorySanitizer (bootstraps instrumented libc++ first; see fault_tolerance/sanitizers/README.md)
make fuzz          # libFuzzer targets under fault_tolerance/fuzz/
make benchmark     # performance/BenchmarkMain.cpp
make packaging-smoke
```

Packaging smoke (binary tarball, ReqPack, optional Docker): `make packaging-smoke`, `make packaging-smoke-docker`.

Fixtures for templates, settings, and batch data: `tests/fixtures/`.
