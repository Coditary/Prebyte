# Security

Targeted tests for sandboxing, escape attempts, and unsafe capability blocking.

- `LuaSandboxE2ETests.cpp` — Lua global removal, `os`/`require`/`loadfile` blocks, metatable escapes, resource limits via AppRunner.

Fuzz coverage for the same surface: `../fault_tolerance/fuzz/LuaSandboxFuzz.cpp`.
