#!/usr/bin/env python3
"""Import a libFuzzer crash input into the deterministic regression corpus."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

VALID_TARGETS = (
    'fuzz_template_lexer',
    'fuzz_template_parser',
    'fuzz_json_parser',
    'fuzz_yaml_parser',
    'fuzz_toml_parser',
    'fuzz_ini_parser',
    'fuzz_env_parser',
    'fuzz_compiled_template_serializer',
    'fuzz_settings_loader',
    'fuzz_include_resolver',
    'fuzz_file_parser',
    'fuzz_lua_chunk',
    'fuzz_render_pbt',
    'fuzz_app_runner',
    'fuzz_batch_render',
    'fuzz_structured_import',
    'fuzz_lua_sandbox',
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def suggest_name(crash_path: Path) -> str:
    script = repo_root() / 'scripts' / 'ci' / 'name_fuzz_corpus_entry.py'
    result = subprocess.run(
        [sys.executable, str(script), '--print-name', str(crash_path)],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def unique_destination(directory: Path, name: str, data: bytes) -> Path:
    candidate = directory / name
    if candidate.exists():
        if candidate.read_bytes() == data:
            return candidate
    else:
        return candidate

    digest = hashlib.sha1(data).hexdigest()[:8]
    for suffix in range(2, 1000):
        candidate = directory / f'{name}_{suffix}'
        if not candidate.exists() or candidate.read_bytes() == data:
            return candidate

    return directory / f'{name}_{digest}'


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True, choices=VALID_TARGETS)
    parser.add_argument('--crash', required=True, type=Path, help='Crash input file from libFuzzer')
    parser.add_argument('--name', help='Optional regression entry name (without path)')
    parser.add_argument('--also-seed', action='store_true', help='Also copy into curated seeds directory')
    args = parser.parse_args()

    crash_path = args.crash.resolve()
    if not crash_path.is_file():
        parser.error(f'crash file not found: {crash_path}')

    root = repo_root()
    regression_dir = root / 'tests' / 'fault_tolerance' / 'fuzz' / 'regression' / args.target
    regression_dir.mkdir(parents=True, exist_ok=True)

    data = crash_path.read_bytes()
    entry_name = args.name or suggest_name(crash_path)
    destination = unique_destination(regression_dir, entry_name, data)
    shutil.copy2(crash_path, destination)

    print(f'Imported regression input: {destination.relative_to(root)}')

    if args.also_seed:
        seeds_dir = root / 'tests' / 'fault_tolerance' / 'fuzz' / 'seeds'
        seed_subdir = {
            'fuzz_template_lexer': 'template',
            'fuzz_template_parser': 'template',
            'fuzz_json_parser': 'json',
            'fuzz_yaml_parser': 'yaml',
            'fuzz_toml_parser': 'toml',
            'fuzz_ini_parser': 'ini',
            'fuzz_env_parser': 'env',
            'fuzz_compiled_template_serializer': 'pbc',
            'fuzz_settings_loader': 'settings',
            'fuzz_include_resolver': 'include',
            'fuzz_file_parser': 'file_parser',
            'fuzz_lua_chunk': 'lua_chunk',
            'fuzz_render_pbt': 'render_pbt',
            'fuzz_app_runner': 'app_runner',
            'fuzz_batch_render': 'batch_render',
            'fuzz_structured_import': 'structured_import',
            'fuzz_lua_sandbox': 'lua_sandbox',
        }[args.target]
        seed_dir = seeds_dir / seed_subdir
        seed_dir.mkdir(parents=True, exist_ok=True)
        seed_destination = unique_destination(seed_dir, f'regression_{entry_name}', data)
        shutil.copy2(crash_path, seed_destination)
        print(f'Also copied to seed: {seed_destination.relative_to(root)}')

    print('Replay with: ./scripts/ci/run_fuzz_regression.sh')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
