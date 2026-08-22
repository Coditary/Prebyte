# Concurrency

End-to-end tests for parallel rendering, shared caches, includes, structured imports, and mixed AppRunner/PrebyteEngine usage.

Unit-level thread-safety checks remain under `../correctness/unit/` (`Engine_concurrent_*`, `PrebyteEngine_concurrent_*`).

CI gate: `make tsan`.
