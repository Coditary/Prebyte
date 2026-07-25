import json
import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
COMPARE_ROOT = ROOT / "tools" / "benchmark_compare"
MANIFEST_PATH = COMPARE_ROOT / "manifest.json"
HISTORY_PATH = COMPARE_ROOT / "history.md"


def load_manifest() -> dict:
    with MANIFEST_PATH.open(encoding="utf-8") as handle:
        return json.load(handle)


def parse_tsv(output: str) -> dict[str, float]:
    results: dict[str, float] = {}
    for line in output.splitlines():
        if not line.strip():
            continue
        name, value = line.split("\t", 1)
        results[name] = float(value)
    return results


def benchmark_external(command: list[str], *, cwd: Path | None = None) -> dict[str, float]:
    completed = subprocess.run(command, capture_output=True, text=True, check=True, cwd=cwd)
    return parse_tsv(completed.stdout)


def benchmark_askama(compare_root: Path, root: Path) -> dict[str, float]:
    target_dir = compare_root / "target"
    env = os.environ.copy()
    env["CARGO_TARGET_DIR"] = str(target_dir)
    completed = subprocess.run(
        ["cargo", "build", "--release", "--quiet"],
        capture_output=True,
        text=True,
        check=True,
        cwd=compare_root,
        env=env,
    )
    if completed.stderr:
        sys.stderr.write(completed.stderr)
    binary_name = "bench_askama.exe" if sys.platform == "win32" else "bench_askama"
    binary = target_dir / "release" / binary_name
    return benchmark_external([str(binary), str(root)])


def fastest_per_case(
    results: dict[str, dict[str, float]], engines: list[str], mode: str, cases: list[dict]
) -> dict[str, float]:
    fastest: dict[str, float] = {}
    for case in cases:
        case_name = case["name"]
        values = [
            results[engine][f"{mode}:{case_name}"]
            for engine in engines
            if f"{mode}:{case_name}" in results[engine]
        ]
        if values:
            fastest[case_name] = min(values)
    return fastest


def format_cell(
    results: dict[str, dict[str, float]],
    engine: str,
    mode: str,
    case: str,
    fastest: dict[str, float],
) -> str:
    key = f"{mode}:{case}"
    value = results[engine].get(key)
    if value is None:
        return "—"
    base = fastest.get(case)
    if base is None or value <= base * 1.001:
        return f"{value:.3f}"
    ratio = value / base
    return f"{value:.3f} ({ratio:.2f}×)"


def format_table(
    results: dict[str, dict[str, float]],
    engines: list[str],
    cases: list[dict],
    mode: str,
    unit: str,
) -> str:
    fastest = fastest_per_case(results, engines, mode, cases)
    header = ["Engine"] + [f"{case['name']} ({unit})" for case in cases]
    lines = [
        "| " + " | ".join(header) + " |",
        "| " + " | ".join(["---"] + [":---:"] * len(cases)) + " |",
    ]
    for engine in engines:
        lines.append(
            "| "
            + " | ".join(
                [engine]
                + [
                    format_cell(results, engine, mode, case["name"], fastest)
                    for case in cases
                ]
            )
            + " |"
        )
    return "\n".join(lines)


def engines_for_mode(results: dict[str, dict[str, float]], mode: dict) -> list[str]:
    if mode.get("prebyte_only"):
        return ["prebyte"]
    return list(results.keys())


def format_mode_section(
    results: dict[str, dict[str, float]], cases: list[dict], mode: dict, unit: str
) -> str:
    mode_id = mode["id"]
    mode_label = mode["label"]
    engines = engines_for_mode(results, mode)
    lines = [f"### {mode_label} ({mode_id})", format_table(results, engines, cases, mode_id, unit), ""]
    if mode.get("prebyte_only"):
        lines.insert(1, "_Prebyte-only mode (BatchProcessor API)._")
        lines.insert(2, "")
    return "\n".join(lines)


def format_group(
    title: str, description: str, results: dict[str, dict[str, float]], group: dict, unit: str
) -> str:
    lines = [f"## {title}", description, ""]
    for mode in group["modes"]:
        lines.append(format_mode_section(results, group["cases"], mode, unit))
    return "\n".join(lines)


def build_report(manifest: dict, results: dict[str, dict[str, float]]) -> str:
    lines = [
        "# Benchmark Compare Report",
        "",
        f"Generated: {datetime.now():%Y-%m-%d %H:%M:%S}",
        "Method: in-process render benchmark, median of 5 runs.",
        "Config: `tools/benchmark_compare/manifest.json` (iterations and cases).",
        "Note: Askama compiles templates at build time; cold measures fresh context per render.",
        "Note: Go/Askama warm-memoized mirrors warm-execute (no output memoization).",
        "Note: Values in parentheses are slower than the fastest engine for that case.",
        "",
    ]

    render_group = manifest["render"]
    lines.append(
        format_group(
            "Render benchmarks",
            render_group["description"],
            results,
            render_group,
            "µs/render",
        )
    )

    batch_group = manifest["batch"]
    lines.append(
        format_group(
            "Batch benchmarks",
            batch_group["description"] + f" Each case uses {batch_group['entries']} entries.",
            results,
            batch_group,
            "µs/entry",
        )
    )
    return "\n".join(lines)


def append_history(report: str) -> None:
    if not HISTORY_PATH.exists():
        HISTORY_PATH.write_text(
            "# Benchmark Compare History\n\n"
            "Each `make compare-benchmark` run appends a timestamped report below.\n",
            encoding="utf-8",
        )
    with HISTORY_PATH.open("a", encoding="utf-8") as handle:
        handle.write("\n---\n\n")
        handle.write(report)
        handle.write("\n")


def main() -> int:
    manifest = load_manifest()
    results: dict[str, dict[str, float]] = {}
    prebyte_bench = Path(sys.argv[1]) if len(sys.argv) >= 2 else COMPARE_ROOT / "bench_prebyte"
    results["prebyte"] = benchmark_external([str(prebyte_bench), str(ROOT)])
    results["go-text-template"] = benchmark_external(["go", "run", str(COMPARE_ROOT / "bench_go.go"), str(ROOT)])
    results["askama"] = benchmark_askama(COMPARE_ROOT, ROOT)

    report = build_report(manifest, results)
    print(report)
    append_history(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
