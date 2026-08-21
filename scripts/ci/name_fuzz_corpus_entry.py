#!/usr/bin/env python3
"""Suggest or apply human-readable names for libFuzzer corpus entries."""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path

SHA1_NAME = re.compile(r"^[0-9a-f]{40}$")
PRINTABLE = re.compile(r"[ -~]{4,}")

FEATURES: tuple[tuple[str, str], ...] = (
    ('include', 'include'),
    ('{{ for', 'for_loop'),
    ('{{ if', 'if'),
    ('lua:block', 'lua_block'),
    ('{{ lua', 'lua'),
    ('{{ fn ', 'function'),
    ('{{ set ', 'set'),
    ('{{ #', 'comment'),
    ('strict_variables', 'strict'),
    ('allow_includes', 'includes'),
    ('error_on_false_input', 'false_input'),
    ('max_include_depth', 'include_depth'),
    ('max_loop_iteration', 'loop_limit'),
    ('output_encoding', 'encoding'),
    ('forbidden_env_vars', 'forbidden_env'),
    ('allow_env', 'allow_env'),
    ('{{ greeting', 'greeting'),
    ('{{ name', 'name'),
)

TARGET_HINTS: dict[str, str] = {
    'fuzz_app_runner': 'app_runner',
    'fuzz_batch_render': 'batch_render',
    'fuzz_render_pbt': 'render_pbt',
    'fuzz_template_lexer': 'template_lexer',
    'fuzz_template_parser': 'template_parser',
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
}


def digest_prefix(data: bytes) -> str:
    return hashlib.sha1(data).hexdigest()[:8]


def decode_text(data: bytes) -> str:
    return data.decode('utf-8', errors='replace')


def extract_snippet(data: bytes) -> str:
    text = decode_text(data)
    marker = text.find('{{')
    if marker != -1:
        return text[marker : marker + 96]
    stripped = text.strip()
    if stripped:
        return stripped[:96]
    for match in PRINTABLE.finditer(decode_text(data)):
        return match.group(0)[:96]
    return 'binary'


def slugify(text: str, max_len: int = 36) -> str:
    slug = re.sub(r'[^a-z0-9]+', '_', text.lower()).strip('_')
    return slug[:max_len] or 'input'


def is_useful_slug(slug: str) -> bool:
    if len(slug) < 4:
        return False
    tokens = [token for token in slug.split('_') if token]
    if not tokens:
        return False
    if len(tokens) > 6:
        return False
    return any(len(token) >= 4 for token in tokens)


def detect_format_tags(data: bytes, text: str) -> list[str]:
    tags: list[str] = []
    stripped = text.lstrip()
    if '{{' in text or '{%' in text:
        tags.append('template')
    elif stripped.startswith('{') or stripped.startswith('['):
        tags.append('json')
    if stripped.startswith('---') or re.search(r'^[A-Za-z0-9_]+:\s', stripped, re.MULTILINE):
        tags.append('yaml')
    if re.search(r'^\[[^\]]+\]', stripped, re.MULTILINE):
        tags.append('toml')
    if re.search(r'^\[[^\]]+\]\s*$', stripped, re.MULTILINE):
        tags.append('ini')
    if '=' in stripped and not tags:
        tags.append('kv')
    if 'return ' in text or 'local ' in text or 'function ' in text:
        tags.append('lua')
    return tags


def detect_feature_tags(text: str) -> list[str]:
    lower = text.lower()
    tags: list[str] = []
    for needle, tag in FEATURES:
        if needle in lower and tag not in tags:
            tags.append(tag)
    return tags


def suggest_name(data: bytes, target_hint: str = '') -> str:
    text = decode_text(data)
    snippet = extract_snippet(data)
    parts: list[str] = []

    if target_hint:
        parts.append(target_hint)

    for tag in detect_format_tags(data, text):
        if tag not in parts:
            parts.append(tag)

    for tag in detect_feature_tags(text):
        if tag not in parts:
            parts.append(tag)

    snippet_slug = slugify(re.sub(r'\{\{|\}\}', ' ', snippet))
    if is_useful_slug(snippet_slug) and snippet_slug not in parts:
        parts.append(snippet_slug)
    elif not any(tag in parts for tag in ('template', 'json', 'yaml', 'toml', 'ini', 'lua', 'kv')):
        parts.append('binary')

    parts.append(digest_prefix(data))
    name = '_'.join(part for part in parts if part)
    name = re.sub(r'_+', '_', name).strip('_')
    return name[:120]


def target_hint_for(path: Path) -> str:
    return TARGET_HINTS.get(path.name, '')


def unique_destination(directory: Path, name: str, data: bytes) -> Path:
    candidate = directory / name
    if candidate.exists():
        if candidate.read_bytes() == data:
            return candidate
    else:
        return candidate

    stem = name
    for suffix in range(2, 1000):
        candidate = directory / f'{stem}_{suffix}'
        if not candidate.exists() or candidate.read_bytes() == data:
            return candidate

    return directory / f'{stem}_{digest_prefix(data)}'


def rename_hash_entries(directory: Path, dry_run: bool = False) -> int:
    renamed = 0
    for path in sorted(directory.iterdir()):
        if not path.is_file() or not SHA1_NAME.match(path.name):
            continue

        data = path.read_bytes()
        new_name = suggest_name(data, target_hint_for(directory))
        destination = unique_destination(directory, new_name, data)
        if destination == path:
            continue

        if dry_run:
            print(f'{path.name} -> {destination.name}')
        else:
            path.rename(destination)
        renamed += 1

    return renamed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('path', nargs='?', type=Path, help='Corpus file or directory')
    parser.add_argument('--print-name', action='store_true', help='Print suggested name for one file')
    parser.add_argument('--rename-dir', action='store_true', help='Rename SHA1 entries in a corpus directory')
    parser.add_argument('--dry-run', action='store_true', help='Show renames without applying them')
    args = parser.parse_args()

    if args.path is None:
        parser.error('path is required')

    path = args.path
    if args.print_name:
        if not path.is_file():
            parser.error('--print-name requires a file path')
        print(suggest_name(path.read_bytes()))
        return 0

    if args.rename_dir:
        if not path.is_dir():
            parser.error('--rename-dir requires a directory path')
        renamed = rename_hash_entries(path, dry_run=args.dry_run)
        if not args.dry_run:
            print(f'Renamed {renamed} entries in {path}', file=sys.stderr)
        return 0

    if path.is_file():
        print(suggest_name(path.read_bytes()))
        return 0

    parser.error('Use --print-name, --rename-dir, or pass a file path')
    return 1


if __name__ == '__main__':
    raise SystemExit(main())
