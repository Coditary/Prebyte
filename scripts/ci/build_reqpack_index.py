#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import tarfile
from pathlib import Path


def sha256_hex(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_metadata(archive_path: Path) -> dict[str, object]:
    with tarfile.open(archive_path, "r") as archive:
        metadata_file = archive.extractfile("metadata.json")
        if metadata_file is None:
            raise RuntimeError(f"metadata.json missing in {archive_path}")
        return json.loads(metadata_file.read().decode("utf-8"))


def build_index(dist_dir: Path) -> dict[str, object]:
    packages: list[dict[str, object]] = []
    for archive_path in sorted(dist_dir.glob("*.rqp")):
        metadata = load_metadata(archive_path)
        packages.append(
            {
                "name": metadata["name"],
                "version": metadata["version"],
                "release": metadata["release"],
                "revision": metadata["revision"],
                "architecture": metadata["architecture"],
                "system": metadata["system"],
                "summary": metadata["summary"],
                "url": metadata["url"],
                "packageSha256": sha256_hex(archive_path),
                "tags": metadata.get("tags", []),
            }
        )

    return {
        "schemaVersion": 1,
        "packages": packages,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Build ReqPack repository index for Prebyte release assets")
    parser.add_argument("--dist-dir", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    dist_dir = (repo_root / args.dist_dir).resolve()
    output_path = (repo_root / args.output).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    index = build_index(dist_dir)
    output_path.write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
