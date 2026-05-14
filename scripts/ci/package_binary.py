#!/usr/bin/env python3

from __future__ import annotations

import argparse
import shutil
import tarfile
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


RUNTIME_PATTERNS = {
    "windows": ["*.dll"],
    "linux": ["*.so", "*.so.*"],
    "macos": ["*.dylib"],
}


def normalized_version(raw: str) -> str:
    return raw[1:] if raw.startswith("v") else raw


def copy_runtime_files(binary_path: Path, stage_bin_dir: Path, platform: str) -> None:
    for pattern in RUNTIME_PATTERNS.get(platform, []):
        for runtime_path in binary_path.parent.glob(pattern):
            if runtime_path.is_file():
                shutil.copy2(runtime_path, stage_bin_dir / runtime_path.name)


def create_archive(stage_dir: Path, output_dir: Path, archive_base: str, platform: str) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    if platform == "windows":
        archive_path = output_dir / f"{archive_base}.zip"
        with ZipFile(archive_path, "w", compression=ZIP_DEFLATED) as archive:
            for path in stage_dir.rglob("*"):
                if path.is_file():
                    archive.write(path, path.relative_to(stage_dir.parent))
        return archive_path

    archive_path = output_dir / f"{archive_base}.tar.gz"
    with tarfile.open(archive_path, "w:gz") as archive:
        archive.add(stage_dir, arcname=stage_dir.name)
    return archive_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Package Prebyte release binary")
    parser.add_argument("--version", required=True)
    parser.add_argument("--platform", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--arch", required=True)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    version = normalized_version(args.version)
    binary_path = Path(args.binary).resolve()
    if not binary_path.is_file():
        raise FileNotFoundError(f"binary not found: {binary_path}")

    repo_root = Path(__file__).resolve().parents[2]
    output_dir = (repo_root / args.output_dir).resolve()
    archive_base = f"prebyte-{version}-{args.platform}-{args.arch}"
    stage_root = output_dir / archive_base
    if stage_root.exists():
        shutil.rmtree(stage_root)

    stage_bin_dir = stage_root / "bin"
    stage_bin_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(binary_path, stage_bin_dir / binary_path.name)
    copy_runtime_files(binary_path, stage_bin_dir, args.platform)

    for doc_name in ("README.md", "LICENSE"):
        shutil.copy2(repo_root / doc_name, stage_root / doc_name)

    archive_path = create_archive(stage_root, output_dir, archive_base, args.platform)
    print(archive_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
