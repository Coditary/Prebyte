import importlib.util
import statistics
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PYDEPS = ROOT / ".cache" / "benchmark_compare" / "pydeps"
EXPECTED_INCLUDE_OUTPUT = "Header for Ada\n\nEnabled\nFooter\n"
CASE_ITERATIONS = {
    "simple-variable": 20000,
    "conditional": 20000,
    "include-if": 5000,
}


def median_us_per_render(render, expected: str, iterations: int) -> float:
    samples = []
    for _ in range(5):
        output = ""
        start = time.perf_counter_ns()
        for _ in range(iterations):
            output = render()
        elapsed = time.perf_counter_ns() - start
        if output != expected:
            raise RuntimeError(f"unexpected output: got={output!r} expected={expected!r}")
        samples.append(elapsed / iterations / 1000.0)
    return statistics.median(samples)


def ensure_python_engines() -> bool:
    have_jinja = importlib.util.find_spec("jinja2") is not None
    have_mako = importlib.util.find_spec("mako") is not None
    if have_jinja and have_mako:
        return True

    PYDEPS.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [sys.executable, "-m", "pip", "install", "--target", str(PYDEPS), "jinja2", "mako"],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    sys.path.insert(0, str(PYDEPS))
    return True


def benchmark_python_engines() -> dict[str, dict[str, float]]:
    if str(PYDEPS) not in sys.path and PYDEPS.exists():
        sys.path.insert(0, str(PYDEPS))
    ensure_python_engines()

    from jinja2 import Environment, FileSystemLoader
    from mako.lookup import TemplateLookup
    from mako.template import Template

    compare_root = ROOT / "tools" / "benchmark_compare"
    results: dict[str, dict[str, float]] = {}

    simple_template_jinja = "Hello {{ name }}"
    conditional_template_jinja = "{% if enabled %}Enabled{% else %}Disabled{% endif %}"
    include_env = Environment(
        loader=FileSystemLoader(str(compare_root / "cases" / "jinja2" / "include_if")),
        autoescape=False,
        cache_size=0,
        keep_trailing_newline=True,
    )
    inline_env = Environment(autoescape=False, cache_size=0)

    results["jinja2"] = {
        "simple-variable": median_us_per_render(
            lambda: inline_env.from_string(simple_template_jinja).render(name="Ada"),
            "Hello Ada",
            CASE_ITERATIONS["simple-variable"],
        ),
        "conditional": median_us_per_render(
            lambda: inline_env.from_string(conditional_template_jinja).render(enabled=True),
            "Enabled",
            CASE_ITERATIONS["conditional"],
        ),
        "include-if": median_us_per_render(
            lambda: include_env.get_template("main.txt").render(name="Ada", enabled=True),
            EXPECTED_INCLUDE_OUTPUT,
            CASE_ITERATIONS["include-if"],
        ),
    }

    simple_template_mako = "Hello ${name}"
    conditional_template_mako = "${'Enabled' if enabled else 'Disabled'}"
    mako_dir = compare_root / "cases" / "mako" / "include_if"

    results["mako"] = {
        "simple-variable": median_us_per_render(
            lambda: Template(simple_template_mako).render(name="Ada"),
            "Hello Ada",
            CASE_ITERATIONS["simple-variable"],
        ),
        "conditional": median_us_per_render(
            lambda: Template(conditional_template_mako).render(enabled=True),
            "Enabled",
            CASE_ITERATIONS["conditional"],
        ),
        "include-if": median_us_per_render(
            lambda: Template(
                filename=str(mako_dir / "main.txt"),
                uri="main.txt",
                lookup=TemplateLookup(directories=[str(mako_dir)], filesystem_checks=True, collection_size=0),
            ).render(name="Ada", enabled=True),
            EXPECTED_INCLUDE_OUTPUT,
            CASE_ITERATIONS["include-if"],
        ),
    }

    return results


def parse_tsv(output: str) -> dict[str, float]:
    results: dict[str, float] = {}
    for line in output.splitlines():
        if not line.strip():
            continue
        name, value = line.split("\t", 1)
        results[name] = float(value)
    return results


def benchmark_external(command: list[str]) -> dict[str, float]:
    completed = subprocess.run(command, capture_output=True, text=True, check=True)
    return parse_tsv(completed.stdout)


def format_table(results: dict[str, dict[str, float]]) -> str:
    cases = ["simple-variable", "conditional", "include-if"]
    engines = list(results.keys())
    lines = [
        "| Engine | simple-variable (us/render) | conditional (us/render) | include-if (us/render) |",
        "| --- | ---: | ---: | ---: |",
    ]
    for engine in engines:
        lines.append(
            f"| {engine} | {results[engine][cases[0]]:.3f} | {results[engine][cases[1]]:.3f} | {results[engine][cases[2]]:.3f} |"
        )
    return "\n".join(lines)


def format_winners(results: dict[str, dict[str, float]]) -> str:
    lines = []
    for case in ["simple-variable", "conditional", "include-if"]:
        ranked = sorted(((engine, values[case]) for engine, values in results.items()), key=lambda item: item[1])
        winner, best = ranked[0]
        lines.append(f"- {case}: {winner} fastest at {best:.3f} us/render")
    return "\n".join(lines)


def main() -> int:
    compare_root = ROOT / "tools" / "benchmark_compare"
    results: dict[str, dict[str, float]] = {}
    prebyte_bench = Path(sys.argv[1]) if len(sys.argv) >= 2 else compare_root / "bench_prebyte"
    results["prebyte"] = benchmark_external([str(prebyte_bench), str(ROOT)])
    results.update(benchmark_python_engines())
    results["go-text-template"] = benchmark_external(["go", "run", str(compare_root / "bench_go.go"), str(ROOT)])

    print("Method: in-process parse+render benchmark, no compiled template cache, median of 5 runs.")
    print("Cases: simple variable, inline conditional, file include + conditional.")
    print()
    print(format_table(results))
    print()
    print("Winners:")
    print(format_winners(results))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
