#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import stat
import subprocess
import tarfile
import tempfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath


RUNTIME_PATTERNS = {
    "linux": ["*.so", "*.so.*"],
    "macos": ["*.dylib"],
}

SYSTEM_BY_PLATFORM = {
    "linux": "linux",
    "macos": "macos",
}

VENDOR = "Coditary"
MAINTAINER_EMAIL = "Matographo@gmail.com"


def normalized_version(raw: str) -> str:
    return raw[1:] if raw.startswith("v") else raw


def is_executable(path: Path) -> bool:
    return bool(path.stat().st_mode & stat.S_IXUSR)


def copy_runtime_files(binary_path: Path, payload_bin_dir: Path, platform: str) -> list[Path]:
    copied: list[Path] = []
    for pattern in RUNTIME_PATTERNS.get(platform, []):
        for runtime_path in sorted(binary_path.parent.glob(pattern)):
            if runtime_path.is_file():
                target_path = payload_bin_dir / runtime_path.name
                shutil.copy2(runtime_path, target_path)
                copied.append(target_path)
    return copied


def compute_payload_directories(files: list[Path], payload_root: Path) -> list[str]:
    directories: set[PurePosixPath] = set()
    for file_path in files:
        relative_path = PurePosixPath(file_path.relative_to(payload_root).as_posix())
        for parent in relative_path.parents:
            if str(parent) == ".":
                continue
            directories.add(parent)
    return [directory.as_posix() for directory in sorted(directories)]


def write_payload_manifest(payload_root: Path, script_path: Path) -> None:
    files = sorted(path for path in payload_root.rglob("*") if path.is_file())
    directories = compute_payload_directories(files, payload_root)
    lines = ["return {", "  directories = {"]
    for directory in directories:
        lines.append(f'    "{directory}",')
    lines.extend(["  },", "  files = {"])

    for file_path in files:
        relative_path = file_path.relative_to(payload_root).as_posix()
        executable = "true" if is_executable(file_path) else "false"
        lines.append(f'    {{ path = "{relative_path}", executable = {executable} }},')

    lines.extend(["  },", "}"])
    script_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def render_template(template_path: Path, output_path: Path, replacements: dict[str, str]) -> None:
    content = template_path.read_text(encoding="utf-8")
    for key, value in replacements.items():
        content = content.replace(key, value)
    output_path.write_text(content, encoding="utf-8")


def copy_template_tree(template_root: Path, control_root: Path) -> None:
    for source_path in sorted(template_root.rglob("*")):
        relative_path = source_path.relative_to(template_root)
        if relative_path == Path("metadata.json.in"):
            continue
        target_path = control_root / relative_path
        if source_path.is_dir():
            target_path.mkdir(parents=True, exist_ok=True)
        elif source_path.is_file():
            target_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source_path, target_path)


def add_tree_to_tar(archive: tarfile.TarFile, root: Path) -> None:
    for path in sorted(root.rglob("*")):
        archive.add(path, arcname=path.relative_to(root).as_posix(), recursive=False)


def create_payload_archive(payload_root: Path, payload_archive_path: Path) -> tuple[int, int]:
    with tarfile.open(payload_archive_path, "w", format=tarfile.USTAR_FORMAT) as archive:
        add_tree_to_tar(archive, payload_root)

    size_installed = sum(path.stat().st_size for path in payload_root.rglob("*") if path.is_file())
    return payload_archive_path.stat().st_size, size_installed


def compress_payload(payload_tar_path: Path, payload_zst_path: Path) -> None:
    subprocess.run(
        ["zstd", "-q", "-f", str(payload_tar_path), "-o", str(payload_zst_path)],
        check=True,
    )


def write_sha256(path: Path, hash_path: Path) -> str:
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    hash_path.write_text(f"{digest} payload/payload.tar.zst\n", encoding="utf-8")
    return digest


def build_metadata(version: str, platform: str, arch: str, archive_name: str, size_compressed: int, size_installed: int) -> dict[str, object]:
    release_tag = f"v{version}"
    build_date = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    return {
        "formatVersion": 1,
        "name": "prebyte",
        "version": version,
        "release": 1,
        "revision": 0,
        "summary": "Prebyte templating CLI",
        "description": "Recursive-descent text templating CLI for Prebyte templates.",
        "license": "MIT",
        "architecture": arch,
        "system": [SYSTEM_BY_PLATFORM[platform]],
        "vendor": VENDOR,
        "maintainerEmail": MAINTAINER_EMAIL,
        "url": f"https://github.com/Coditary/Prebyte/releases/download/{release_tag}/{archive_name}",
        "homepage": "https://github.com/Coditary/Prebyte",
        "sourceUrl": "https://github.com/Coditary/Prebyte",
        "buildDate": build_date,
        "tags": ["cli", "templating", "prebyte"],
        "payload": {
            "path": "payload/payload.tar.zst",
            "archive": "tar",
            "compression": "zstd",
            "hashAlgorithm": "sha256",
            "hashFile": "hashes/payload.sha256",
            "sizeCompressed": size_compressed,
            "sizeInstalledExpected": size_installed,
        },
    }


def metadata_replacements(metadata: dict[str, object]) -> dict[str, str]:
    payload = metadata["payload"]
    return {
        "@VERSION@": json.dumps(metadata["version"]),
        "@ARCHITECTURE@": json.dumps(metadata["architecture"]),
        "@SYSTEM@": json.dumps(metadata["system"][0]),
        "@URL@": json.dumps(metadata["url"]),
        "@BUILD_DATE@": json.dumps(metadata["buildDate"]),
        "@SIZE_COMPRESSED@": str(payload["sizeCompressed"]),
        "@SIZE_INSTALLED_EXPECTED@": str(payload["sizeInstalledExpected"]),
    }


def create_rqp_archive(control_root: Path, archive_path: Path) -> None:
    with tarfile.open(archive_path, "w", format=tarfile.USTAR_FORMAT) as archive:
        add_tree_to_tar(archive, control_root)


def validate_rqp_archive(archive_path: Path, payload_sha256: str) -> None:
    required_entries = {
        "metadata.json",
        "reqpack.lua",
        "scripts/layout.lua",
        "scripts/install.lua",
        "scripts/remove.lua",
        "scripts/payload_files.lua",
        "hashes/payload.sha256",
        "payload/payload.tar.zst",
    }
    allowed_top_levels = {"metadata.json", "reqpack.lua", "scripts", "hashes", "payload"}

    with tarfile.open(archive_path, "r") as archive:
        names = {member.name.rstrip("/") for member in archive.getmembers() if member.name.rstrip("/")}
        missing_entries = required_entries - names
        if missing_entries:
            missing_display = ", ".join(sorted(missing_entries))
            raise RuntimeError(f"rqp archive missing entries: {missing_display}")

        for name in names:
            top_level = PurePosixPath(name).parts[0]
            if top_level not in allowed_top_levels:
                raise RuntimeError(f"unexpected top-level archive entry: {name}")

        metadata = json.loads(archive.extractfile("metadata.json").read().decode("utf-8"))
        if metadata.get("payload", {}).get("path") != "payload/payload.tar.zst":
            raise RuntimeError("metadata payload path mismatch")

        hash_content = archive.extractfile("hashes/payload.sha256").read().decode("utf-8")
        expected_hash_line = f"{payload_sha256} payload/payload.tar.zst\n"
        if hash_content != expected_hash_line:
            raise RuntimeError("payload hash file content mismatch")


def main() -> int:
    parser = argparse.ArgumentParser(description="Package Prebyte as ReqPack .rqp archive")
    parser.add_argument("--version", required=True)
    parser.add_argument("--platform", required=True, choices=["linux", "macos"])
    parser.add_argument("--arch", required=True, choices=["x86_64", "aarch64"])
    parser.add_argument("--binary", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    version = normalized_version(args.version)
    binary_path = Path(args.binary).resolve()
    if not binary_path.is_file():
        raise FileNotFoundError(f"binary not found: {binary_path}")

    repo_root = Path(__file__).resolve().parents[2]
    template_root = repo_root / "packaging" / "reqpack" / "package"
    output_dir = (repo_root / args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    for required_path in (
        template_root / "metadata.json.in",
        template_root / "reqpack.lua",
        template_root / "scripts" / "layout.lua",
        template_root / "scripts" / "install.lua",
        template_root / "scripts" / "remove.lua",
    ):
        if not required_path.is_file():
            raise FileNotFoundError(f"reqpack template file not found: {required_path}")

    archive_name = f"prebyte-{version}-{args.platform}-{args.arch}.rqp"
    archive_path = output_dir / archive_name

    with tempfile.TemporaryDirectory(prefix=f"prebyte-rqp-{args.platform}-{args.arch}-", dir=output_dir) as temp_dir:
        temp_root = Path(temp_dir)
        payload_root = temp_root / "payload-tree"
        payload_bin_dir = payload_root / "bin"
        payload_doc_dir = payload_root / "share" / "doc" / "prebyte"
        control_root = temp_root / "control"
        control_scripts_dir = control_root / "scripts"
        control_hashes_dir = control_root / "hashes"
        control_payload_dir = control_root / "payload"

        payload_bin_dir.mkdir(parents=True, exist_ok=True)
        payload_doc_dir.mkdir(parents=True, exist_ok=True)
        control_hashes_dir.mkdir(parents=True, exist_ok=True)
        control_payload_dir.mkdir(parents=True, exist_ok=True)
        copy_template_tree(template_root, control_root)

        binary_target = payload_bin_dir / "prebyte"
        shutil.copy2(binary_path, binary_target)
        copy_runtime_files(binary_path, payload_bin_dir, args.platform)
        for doc_name in ("README.md", "LICENSE"):
            shutil.copy2(repo_root / doc_name, payload_doc_dir / doc_name)

        write_payload_manifest(payload_root, control_scripts_dir / "payload_files.lua")

        payload_tar_path = control_payload_dir / "payload.tar"
        _, size_installed = create_payload_archive(payload_root, payload_tar_path)
        payload_zst_path = control_payload_dir / "payload.tar.zst"
        compress_payload(payload_tar_path, payload_zst_path)
        size_compressed = payload_zst_path.stat().st_size
        payload_sha256 = write_sha256(payload_zst_path, control_hashes_dir / "payload.sha256")
        payload_tar_path.unlink()

        metadata = build_metadata(version, args.platform, args.arch, archive_name, size_compressed, size_installed)
        render_template(
            template_root / "metadata.json.in",
            control_root / "metadata.json",
            metadata_replacements(metadata),
        )

        if archive_path.exists():
            archive_path.unlink()
        create_rqp_archive(control_root, archive_path)
        validate_rqp_archive(archive_path, payload_sha256)

    print(archive_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
