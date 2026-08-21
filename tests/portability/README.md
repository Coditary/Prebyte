# Portability

Validates the shipped CLI binary via real subprocesses (not in-process AppRunner calls).

- `cli/CliBinaryE2ETests.cpp` — help, version, render, batch, list/explain modes, diagnostics on stderr.

Requires `prebyte` to be built (`add_dependencies(prebyte_tests prebyte)`). Override binary path with `PREBYTE_CLI_BINARY` if needed.

Multi-platform build coverage: CI `build-test` matrix (Linux/macOS/Windows × x86_64/ARM64).
