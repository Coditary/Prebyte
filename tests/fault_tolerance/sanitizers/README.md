# Sanitizer runs

Sanitizer coverage is not a separate source tree. The full `prebyte_tests` binary is rebuilt and executed under Clang instrumentation:

| Sanitizer | Local command | CI job |
| --- | --- | --- |
| Address + UndefinedBehavior | `make sanitize` | `asan-ubsan-linux-x86_64` |
| Thread | `make tsan` | `tsan-linux-x86_64` |
| Memory | `make msan` | `msan-linux-x86_64` |

Entry point: `scripts/ci/run_sanitize_tests.sh`.

Correctness tests under `tests/correctness/`, plus security, concurrency, portability, and regression tests, all participate in these runs.
