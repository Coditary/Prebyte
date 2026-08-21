# Tests

Tests are grouped by non-functional requirement (NFR). Shared assets live in `fixtures/`, `harness/`, and `support/`.

| NFR | Directory | What runs here |
| --- | --- | --- |
| Correctness | `correctness/` | Unit, integration, and property tests (`prebyte_tests`) |
| Fault tolerance | `fault_tolerance/` | Fuzzers, fuzz regressions, sanitizer guidance |
| Security | `security/` | Sandbox and hardening end-to-end tests |
| Concurrency | `concurrency/` | Parallel render and cache stability tests |
| Portability | `portability/` | Real CLI subprocess tests across the built binary |
| Performance | `performance/` | Benchmark driver and timing history |

## Running

```bash
cmake --build --preset coverage --target prebyte_tests prebyte
ctest --preset coverage
make coverage      # correctness + coverage gate
make sanitize      # ASan/UBSan over the full test binary
make tsan          # ThreadSanitizer
make msan          # MemorySanitizer
make fuzz          # libFuzzer targets under fault_tolerance/fuzz/
make benchmark     # performance/BenchmarkMain.cpp
```

Fixtures for templates, settings, and batch data: `tests/fixtures/`.
