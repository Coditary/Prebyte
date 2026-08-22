# Benchmark Compare

Cross-engine benchmark harness for Prebyte, Go `text/template`, and Rust Askama.

Manual tool only — not run in CI. Use locally to compare engines and track trends in `history.md`.

## Layout

```
tools/benchmark_compare/
├── manifest.json          # Single source of truth: cases, iterations, modes
├── run_compare.py         # Orchestrator, report printer, history appender
├── history.md             # Appended report history (created on first run)
├── bench_prebyte.cpp      # Prebyte micro-benchmark binary (built by CMake)
├── bench_go.go            # Go harness
├── src/main.rs            # Askama harness
├── cases/
│   ├── prebyte/           # Prebyte file fixtures
│   ├── gotemplate/        # Go file fixtures
│   └── batch/             # Shared batch template + JSON input
└── templates/             # Askama compile-time templates
```

## Run

From the repo root:

```bash
make compare-benchmark
```

Or directly:

```bash
cmake --build --preset dev --target compare-benchmark
```

## Configuration

Edit `manifest.json` to change case names, iteration counts, or mode groups. All three harnesses read this file at runtime — no need to touch C++/Go/Rust when tuning iterations.

## Benchmark groups

### 1. Render (`manifest.json` → `render`)

Single-template renders. All three engines participate.

| Mode | Meaning |
| --- | --- |
| `cold` | Fresh parse/compile path each render |
| `warm-execute` | Template prepared once, execute again |
| `warm-memoized` | Repeat render after output memoization is primed |
| `mt-*` | Same as above, iterations split across CPU threads |

Cases: `simple-variable`, `conditional`, `include-if`, `control-flow`.

### 2. Batch (`manifest.json` → `batch`)

Eight variable sets rendered from one template (`cases/batch/template.txt` + `data.json`).

| Mode | Prebyte | Go / Askama |
| --- | --- | --- |
| `batch-warm` | Template + JSON parsed once, entries re-rendered | — |
| `batch-cold` | Full `BatchProcessor` path each iteration | — |
| `sequential-warm` | Warm state, 8 single renders per iteration | 8 warm renders per iteration |
| `sequential-cold` | 8 cold renders per iteration | 8 cold renders per iteration |

Results are reported as **microseconds per entry**.

## Output format

Each harness prints TSV lines:

```
mode:case<TAB>microseconds_per_render_or_entry
```

`run_compare.py` collects these lines, computes medians, prints grouped markdown tables with relative slowdown vs. the fastest engine, and appends the report to `history.md`.

## Debug a single metric

```bash
./build-cmake/dev/benchmark_compare_prebyte . batch-warm:batch-variable
./build-cmake/dev/benchmark_compare_prebyte . simple-variable
```

## Internal history

`make benchmark` appends single-render and batch CLI timings to `tests/performance/history.md` via `tests/performance/BenchmarkMain.cpp`.

`make compare-benchmark` appends cross-engine reports to `tools/benchmark_compare/history.md`.
