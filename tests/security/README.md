# Security

Targeted tests for sandboxing, escape attempts, and unsafe capability blocking.

- `LuaSandboxE2ETests.cpp` — Lua global removal, `os`/`require`/`loadfile` blocks, metatable escapes, resource limits via AppRunner.
- `IncludeSecurityE2ETests.cpp` — include cycles, disabled includes, depth limits, missing/absolute/overlong paths.

Fuzz coverage for the same surfaces: `../fault_tolerance/fuzz/LuaSandboxFuzz.cpp`, `../fault_tolerance/fuzz/IncludeResolverFuzz.cpp`.
