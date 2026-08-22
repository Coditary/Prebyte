#!/usr/bin/env python3
"""Run internal benchmarks and fail when cases exceed committed baselines."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def load_baselines(path: Path) -> dict[str, int]:
    baselines: dict[str, int] = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith('#'):
            continue
        name, _, value = stripped.partition('=')
        if not name or not value:
            raise ValueError(f'invalid baseline line: {line!r}')
        baselines[name.strip()] = int(value.strip())
    return baselines


def parse_latest_section(history_text: str) -> dict[str, int]:
    rows: dict[str, int] = {}
    in_table = False
    for line in history_text.splitlines():
        if line.startswith('### '):
            in_table = False
            continue
        if line.startswith('| Case |'):
            in_table = True
            continue
        if not in_table or not line.startswith('|'):
            continue
        if line.startswith('| ---'):
            continue

        columns = [column.strip() for column in line.strip('|').split('|')]
        if len(columns) < 2:
            continue
        case_name = columns[0]
        time_text = columns[1]
        if not case_name or case_name == 'Case':
            continue
        rows[case_name] = int(time_text)
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--benchmark-binary',
        type=Path,
        help='Path to prebyte_benchmarks (default: build-cmake/dev/prebyte_benchmarks)',
    )
    parser.add_argument(
        '--baselines',
        type=Path,
        default=repo_root() / 'tests' / 'performance' / 'baselines.txt',
        help='Baseline limits file',
    )
    args = parser.parse_args()

    root = repo_root()
    benchmark_binary = args.benchmark_binary or (root / 'build-cmake' / 'dev' / 'prebyte_benchmarks')
    if not benchmark_binary.is_file():
        parser.error(f'benchmark binary not found: {benchmark_binary}')

    baselines = load_baselines(args.baselines)
    with tempfile.TemporaryDirectory(prefix='prebyte-benchmark-gate-') as temp_dir:
        history_path = Path(temp_dir) / 'history.md'
        subprocess.run(
            [str(benchmark_binary)],
            check=True,
            cwd=root,
            env={**os.environ, 'PREBYTE_BENCHMARK_HISTORY': str(history_path)},
        )
        measured = parse_latest_section(history_path.read_text(encoding='utf-8'))

    failures: list[str] = []
    for case_name, limit in baselines.items():
        if case_name not in measured:
            failures.append(f'missing benchmark case: {case_name}')
            continue
        actual = measured[case_name]
        if actual > limit:
            failures.append(f'{case_name}: {actual}us exceeds baseline {limit}us')

    if failures:
        print('Benchmark regression gate failed:', file=sys.stderr)
        for failure in failures:
            print(f'  - {failure}', file=sys.stderr)
        return 1

    print(f'Benchmark regression gate passed for {len(baselines)} case(s).')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
