#!/usr/bin/env python3
"""Smoke-test release packaging artifacts and optional Docker images."""

from __future__ import annotations

import argparse
import json
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path


class SmokeError(RuntimeError):
    pass


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def read_binary_version(binary: Path) -> str:
    output = run_packaged_binary(binary, ["--version"]).strip()
    if output.startswith("v"):
        return output[1:]
    return output


def read_project_version(root: Path) -> str:
    cmake_lists = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\([^\n]*VERSION\s+([^\s)]+)", cmake_lists)
    if not match:
        raise SmokeError("failed to read project version from CMakeLists.txt")
    return match.group(1)


def detect_platform() -> str:
    system = platform.system().lower()
    if system == "linux":
        return "linux"
    if system == "darwin":
        return "macos"
    if system == "windows":
        return "windows"
    raise SmokeError(f"unsupported host platform for packaging smoke tests: {system}")


def detect_arch() -> str:
    machine = platform.machine().lower()
    if machine in {"x86_64", "amd64"}:
        return "x86_64"
    if machine in {"aarch64", "arm64"}:
        return "aarch64"
    raise SmokeError(f"unsupported host architecture for packaging smoke tests: {machine}")


def run_command(command: list[str], *, cwd: Path | None = None, input_text: str | None = None) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            command,
            cwd=str(cwd) if cwd is not None else None,
            input=input_text,
            capture_output=True,
            text=True,
            check=False,
            timeout=120,
        )
    except subprocess.TimeoutExpired as error:
        raise SmokeError(f"command timed out: {' '.join(command)}") from error


def require_success(result: subprocess.CompletedProcess[str], context: str) -> str:
    if result.returncode != 0:
        raise SmokeError(
            f"{context} failed (exit {result.returncode})\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return result.stdout


def run_packaged_binary(binary_path: Path, args: list[str], *, cwd: Path | None = None, input_text: str | None = None) -> str:
    result = run_command([str(binary_path), *args], cwd=cwd, input_text=input_text)
    return require_success(result, f"running packaged binary {binary_path}")


def package_binary(root: Path, binary: Path, dist_dir: Path, version: str, platform_name: str, arch: str) -> Path:
    script = root / "scripts" / "ci" / "package_binary.py"
    result = run_command(
        [
            sys.executable,
            str(script),
            "--version",
            version,
            "--platform",
            platform_name,
            "--arch",
            arch,
            "--binary",
            str(binary),
            "--output-dir",
            str(dist_dir.relative_to(root)),
        ],
        cwd=root,
    )
    archive_path = Path(require_success(result, "package_binary.py").strip())
    if not archive_path.is_file():
        raise SmokeError(f"binary package not created: {archive_path}")
    return archive_path


def extract_binary_archive(archive_path: Path, extract_dir: Path, platform_name: str) -> Path:
    if platform_name == "windows":
        with zipfile.ZipFile(archive_path) as archive:
            archive.extractall(extract_dir)
        matches = sorted(extract_dir.glob("prebyte-*/bin/prebyte.exe"))
        if not matches:
            raise SmokeError(f"packaged prebyte binary not found in {archive_path}")
        return matches[0]

    with tarfile.open(archive_path, "r:gz") as archive:
        archive.extractall(extract_dir, filter="data")

    matches = sorted(extract_dir.glob("prebyte-*/bin/prebyte"))
    if not matches:
        raise SmokeError(f"packaged prebyte binary not found in {archive_path}")
    return matches[0]


def smoke_binary_package(root: Path, binary: Path, dist_dir: Path, version: str, platform_name: str, arch: str) -> None:
    archive_path = package_binary(root, binary, dist_dir, version, platform_name, arch)
    with tempfile.TemporaryDirectory(prefix="prebyte-binary-smoke-") as temp_dir:
        packaged_binary = extract_binary_archive(archive_path, Path(temp_dir), platform_name)
        output = run_packaged_binary(packaged_binary, ["--version"])
        if version not in output and f"v{version}" not in output:
            raise SmokeError(f"packaged binary --version missing {version}: {output!r}")

        fixture_dir = root / "tests" / "fixtures" / "render_simple"
        rendered = run_packaged_binary(
            packaged_binary,
            ["input.txt", "-Dname=Packaged"],
            cwd=fixture_dir,
        )
        if rendered != "Hello Packaged\n":
            raise SmokeError(f"unexpected packaged render output: {rendered!r}")


def package_reqpack(root: Path, binary: Path, dist_dir: Path, version: str, platform_name: str, arch: str) -> Path:
    if platform_name not in {"linux", "macos"}:
        raise SmokeError(f"reqpack packaging is only supported on linux/macos, not {platform_name}")
    if shutil.which("zstd") is None:
        raise SmokeError("zstd is required for reqpack packaging smoke tests")

    script = root / "scripts" / "ci" / "package_reqpack.py"
    result = run_command(
        [
            sys.executable,
            str(script),
            "--version",
            version,
            "--platform",
            platform_name,
            "--arch",
            arch,
            "--binary",
            str(binary),
            "--output-dir",
            str(dist_dir.relative_to(root)),
        ],
        cwd=root,
    )
    archive_path = Path(require_success(result, "package_reqpack.py").strip())
    if not archive_path.is_file():
        raise SmokeError(f"reqpack archive not created: {archive_path}")
    return archive_path


def extract_reqpack_payload(archive_path: Path, extract_dir: Path) -> Path:
    payload_zst = extract_dir / "payload.tar.zst"
    payload_tar = extract_dir / "payload.tar"
    with tarfile.open(archive_path, "r") as archive:
        required_entries = {
            "metadata.json",
            "reqpack.lua",
            "payload/payload.tar.zst",
            "hashes/payload.sha256",
        }
        names = {member.name.rstrip("/") for member in archive.getmembers()}
        missing = required_entries - names
        if missing:
            raise SmokeError(f"reqpack archive missing entries: {', '.join(sorted(missing))}")

        payload_member = archive.getmember("payload/payload.tar.zst")
        with archive.extractfile(payload_member) as payload_stream:
            if payload_stream is None:
                raise SmokeError("failed to read reqpack payload stream")
            payload_zst.write_bytes(payload_stream.read())

    require_success(
        run_command(["zstd", "-d", "-f", str(payload_zst), "-o", str(payload_tar)]),
        "decompressing reqpack payload",
    )

    install_root = extract_dir / "installed"
    install_root.mkdir(parents=True, exist_ok=True)
    with tarfile.open(payload_tar, "r") as payload_archive:
        payload_archive.extractall(install_root, filter="data")

    matches = sorted(install_root.glob("bin/prebyte"))
    if not matches:
        raise SmokeError(f"reqpack payload missing bin/prebyte in {archive_path}")
    return matches[0]


def smoke_reqpack_package(root: Path, binary: Path, dist_dir: Path, version: str, platform_name: str, arch: str) -> Path:
    archive_path = package_reqpack(root, binary, dist_dir, version, platform_name, arch)
    with tempfile.TemporaryDirectory(prefix="prebyte-reqpack-smoke-") as temp_dir:
        packaged_binary = extract_reqpack_payload(archive_path, Path(temp_dir))
        output = run_packaged_binary(packaged_binary, ["--version"])
        if version not in output and f"v{version}" not in output:
            raise SmokeError(f"reqpack payload --version missing {version}: {output!r}")

        fixture_dir = root / "tests" / "fixtures" / "render_simple"
        rendered = run_packaged_binary(
            packaged_binary,
            ["input.txt", "-Dname=ReqPack"],
            cwd=fixture_dir,
        )
        if rendered != "Hello ReqPack\n":
            raise SmokeError(f"unexpected reqpack render output: {rendered!r}")
    return archive_path


def smoke_reqpack_index(root: Path, dist_dir: Path, version: str, platform_name: str, arch: str) -> None:
    index_path = dist_dir / "index.json"
    script = root / "scripts" / "ci" / "build_reqpack_index.py"
    result = run_command(
        [
            sys.executable,
            str(script),
            "--dist-dir",
            str(dist_dir.relative_to(root)),
            "--output",
            str(index_path.relative_to(root)),
        ],
        cwd=root,
    )
    require_success(result, "build_reqpack_index.py")
    if not index_path.is_file():
        raise SmokeError(f"reqpack index not created: {index_path}")

    index = json.loads(index_path.read_text(encoding="utf-8"))
    packages = index.get("packages", [])
    if not packages:
        raise SmokeError("reqpack index does not list any packages")

    matching = [
        package
        for package in packages
        if package.get("name") == "prebyte"
        and package.get("version") == version
        and package.get("architecture") == arch
        and platform_name in package.get("system", [])
    ]
    if not matching:
        raise SmokeError(
            f"reqpack index missing package for prebyte {version} {platform_name}/{arch}: {packages!r}"
        )


def smoke_docker_image(root: Path, version: str) -> None:
    if shutil.which("docker") is None:
        raise SmokeError("docker is required for docker packaging smoke tests")

    image = "prebyte-packaging-smoke:local"
    build = run_command(
        [
            "docker",
            "build",
            "-t",
            image,
            "--build-arg",
            f"PREBYTE_VERSION={version}",
            str(root),
        ]
    )
    require_success(build, "docker build")

    version_output = require_success(
        run_command(["docker", "run", "--rm", image, "--version"]),
        "docker image --version",
    )
    if version not in version_output and f"v{version}" not in version_output:
        raise SmokeError(f"docker image --version missing {version}: {version_output!r}")

    fixture_dir = root / "tests" / "fixtures" / "render_simple"
    render = require_success(
        run_command(
            [
                "docker",
                "run",
                "--rm",
                "-v",
                f"{fixture_dir}:/work:ro,z",
                "-w",
                "/work",
                image,
                "input.txt",
                "-Dname=Docker",
            ]
        ),
        "docker image render",
    )
    if render != "Hello Docker\n":
        raise SmokeError(f"unexpected docker render output: {render!r}")

    stdin_render = require_success(
        run_command(
            ["docker", "run", "--rm", "-i", image, "-Dname=Stdin", "--"],
            input_text="Hello {{ name }}!\n",
        ),
        "docker stdin render",
    )
    if stdin_render != "Hello Stdin!\n":
        raise SmokeError(f"unexpected docker stdin render output: {stdin_render!r}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, help="Path to built prebyte binary")
    parser.add_argument("--repo-root", type=Path, default=repo_root())
    parser.add_argument("--version", help="Package version (default: project version)")
    parser.add_argument("--platform", choices=["linux", "macos", "windows"], help="Target package platform")
    parser.add_argument("--arch", choices=["x86_64", "aarch64"], help="Target package architecture")
    parser.add_argument(
        "--checks",
        nargs="+",
        choices=["binary", "reqpack", "index", "docker"],
        default=["binary", "reqpack", "index"],
        help="Smoke checks to run",
    )
    parser.add_argument(
        "--dist-dir",
        type=Path,
        help="Directory for generated packages (default: temp dir under repo dist/)",
    )
    args = parser.parse_args()

    root = args.repo_root.resolve()
    platform_name = args.platform or detect_platform()
    arch = args.arch or detect_arch()

    binary_checks = {"binary", "reqpack", "index"}
    if binary_checks.intersection(args.checks) and args.binary is None:
        parser.error("--binary is required for binary, reqpack, and index checks")

    binary: Path | None = None
    if args.binary is not None:
        binary = args.binary.resolve()
        if not binary.is_file():
            parser.error(f"binary not found: {binary}")

    version = args.version
    if version is None and binary is not None:
        version = read_binary_version(binary)
    if version is None:
        version = read_project_version(root)

    dist_dir = args.dist_dir
    temp_dist: tempfile.TemporaryDirectory[str] | None = None
    if dist_dir is None:
        dist_parent = root / "dist"
        dist_parent.mkdir(parents=True, exist_ok=True)
        temp_dist = tempfile.TemporaryDirectory(prefix="prebyte-packaging-smoke-", dir=dist_parent)
        dist_dir = Path(temp_dist.name)
    else:
        dist_dir = dist_dir.resolve()
        dist_dir.mkdir(parents=True, exist_ok=True)

    try:
        if "binary" in args.checks:
            print("Smoke check: binary release archive")
            assert binary is not None
            smoke_binary_package(root, binary, dist_dir, version, platform_name, arch)

        reqpack_archive: Path | None = None
        if "reqpack" in args.checks:
            assert binary is not None
            if platform_name == "windows":
                print("Skipping reqpack smoke on windows")
            else:
                print("Smoke check: reqpack archive")
                reqpack_archive = smoke_reqpack_package(root, binary, dist_dir, version, platform_name, arch)

        if "index" in args.checks:
            assert binary is not None
            if platform_name == "windows":
                print("Skipping reqpack index smoke on windows")
            else:
                if reqpack_archive is None:
                    reqpack_archive = package_reqpack(root, binary, dist_dir, version, platform_name, arch)
                print("Smoke check: reqpack index")
                smoke_reqpack_index(root, dist_dir, version, platform_name, arch)

        if "docker" in args.checks:
            print("Smoke check: docker image")
            smoke_docker_image(root, version)
    except SmokeError as error:
        print(f"Packaging smoke test failed: {error}", file=sys.stderr)
        return 1

    print("Packaging smoke tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
