# Packaging smoke tests

End-to-end checks that release artifacts produced after a build actually run.

- `PackagingSmokeTests.cpp` — invokes `scripts/ci/smoke_packaging.py` against the built `prebyte` binary.
- Binary tarball (`package_binary.py`) on all platforms.
- ReqPack archive + repository index (`package_reqpack.py`, `build_reqpack_index.py`) on Linux/macOS.
- Optional Docker image smoke when `PREBYTE_SMOKE_DOCKER=1`.

Run locally:

```bash
make packaging-smoke
make packaging-smoke-docker   # requires Docker
```
