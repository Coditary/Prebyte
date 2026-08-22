# Sanitizer runs

Sanitizer coverage is not a separate source tree. The full `prebyte_tests` binary is rebuilt and executed under Clang instrumentation:

| Sanitizer | Local command | CI job | Notes |
| --- | --- | --- | --- |
| Address + UndefinedBehavior | `make sanitize` | `asan-ubsan-linux-x86_64` | Works with the system C++ standard library |
| Thread | `make tsan` | `tsan-linux-x86_64` | Works with the system C++ standard library |
| Memory | `make msan` | `msan-linux-x86_64` | Requires an MSan-instrumented **libc++** (bootstrapped automatically) |

Entry point: `scripts/ci/run_sanitize_tests.sh`.

## MemorySanitizer setup

MSan needs every library linked into the binary to be compiled with `-fsanitize=memory`. The system `libstdc++` on Linux is not, which produces false positives in tests that touch `std::filesystem`, `std::chrono`, and similar APIs.

`make msan` therefore:

1. builds/installs an instrumented libc++ via `scripts/ci/bootstrap_msan_libcxx.sh` (cached under `~/.cache/prebyte-msan-libcxx` by default)
2. configures the `msan` CMake preset with `-DPREBYTE_MSAN_LIBCXX_PREFIX=...`
3. links `prebyte_tests` against that libc++

Prerequisites: `clang`, `clang++`, `cmake`, `ninja`, and `git`.

The bootstrap builds the LLVM runtimes `libcxx` and `libc++abi` with `-fsanitize=memory` — not the full LLVM toolchain — so it completes in a few minutes and is cached between runs.

`libunwind` is deliberately **not** part of the instrumented build. Its register context is populated by assembly (`__unw_getcontext`), which MSan cannot track, so an instrumented libunwind reports `use-of-uninitialized-value` inside `UnwindCursor::getReg` on every C++ exception throw — and because MSan itself uses the unwinder to print the report, the failure recurses into a `stack-overflow`/`nested bug` abort (upstream: [llvm/llvm-project#84348](https://github.com/llvm/llvm-project/issues/84348)). The bootstrap therefore sets `LIBCXXABI_USE_LLVM_UNWINDER=OFF`, and exception unwinding goes through the system `libgcc_s`, which needs no instrumentation.

`make ci-fast` and the default `make ci-full` gate on ASan/TSan only. Opt in to MSan locally with:

```bash
PREBYTE_ALL_CHECKS_MSAN=1 make ci-full
```

Correctness tests under `tests/correctness/`, plus security, concurrency, portability, and regression tests, all participate in these runs.
