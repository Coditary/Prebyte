# Performance

- `BenchmarkMain.cpp` — internal render and batch timing cases.
- `history.md` — append-only timing history from `make benchmark`.
- `baselines.txt` — per-case upper bounds for `make benchmark-gate`.

Cross-engine comparison (Go `text/template`, Rust Askama) lives in `tools/benchmark_compare/` and is not part of CI.
