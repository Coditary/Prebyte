# ReqPack Package Design for Prebyte

## Goal

Add ReqPack-native `.rqp` package generation for Prebyte releases.

Scope for v1:

- build one `.rqp` package per supported ReqPack target
- support Linux and macOS only
- keep package contents CLI-only
- build packages automatically in GitHub Actions on tag releases
- upload `.rqp` files as additional GitHub Release assets

Out of scope for v1:

- Windows ReqPack packages
- ReqPack repository index JSON
- dev packages with headers or libraries
- separate remove/update hooks
- replacing existing `.tar.gz`, `.zip`, or Docker release flow

## Supported Targets

ReqPack packages will be generated only for these release targets:

- `linux-x86_64`
- `linux-aarch64`
- `macos-x86_64`
- `macos-aarch64`

Windows release jobs remain unchanged except that they do not build `.rqp` assets.

## Package Model

Prebyte will publish one target-specific `.rqp` package per supported Linux/macOS matrix entry.

Package identity:

- package name: `prebyte`
- archive name pattern: `prebyte-<version>-<platform>-<arch>.rqp`

Examples:

- `prebyte-1.0.0-linux-x86_64.rqp`
- `prebyte-1.0.0-linux-aarch64.rqp`
- `prebyte-1.0.0-macos-x86_64.rqp`
- `prebyte-1.0.0-macos-aarch64.rqp`

This model mirrors the existing release matrix and avoids any multi-platform wrapper logic inside ReqPack packages.

## Installed Contents

Each `.rqp` package installs only CLI runtime files:

- `bin/prebyte`
- `share/doc/prebyte/LICENSE`
- `share/doc/prebyte/README.md`

No headers, libraries, tests, or benchmark files are included.

For macOS and Linux, the installed executable path is always `bin/prebyte`.

## Repository Layout Changes

Add these new files:

```text
packaging/reqpack/
  package/
    metadata.json.in
    reqpack.lua
    scripts/
      layout.lua
      install.lua
      remove.lua

scripts/ci/package_reqpack.py
docs/superpowers/specs/2026-05-14-reqpack-package-design.md
```

Purpose of each file:

- `packaging/reqpack/package/metadata.json.in`: metadata template rendered during package build
- `packaging/reqpack/package/reqpack.lua`: ReqPack hook manifest
- `packaging/reqpack/package/scripts/layout.lua`: shared package path helpers and constants
- `packaging/reqpack/package/scripts/install.lua`: install hook used inside built packages
- `packaging/reqpack/package/scripts/remove.lua`: remove hook used during uninstall and update replacement
- `scripts/ci/package_reqpack.py`: CI builder that assembles `.rqp` archives from already-built release binaries

`metadata.json` will be rendered from `metadata.json.in` by `package_reqpack.py` so version, architecture, URLs, and payload sizes stay in sync with each build while package structure remains visible in the repository.

## .rqp Package Layout

Each produced `.rqp` archive will follow ReqPack-controlled top-level layout:

```text
metadata.json
reqpack.lua
scripts/layout.lua
scripts/install.lua
scripts/remove.lua
hashes/payload.sha256
payload/payload.tar.zst
```

The compressed payload archive will contain this install tree:

```text
bin/prebyte
share/doc/prebyte/LICENSE
share/doc/prebyte/README.md
```

## Metadata Generation

`package_reqpack.py` will generate `metadata.json` with these fields:

Required base fields:

- `formatVersion: 1`
- `name: "prebyte"`
- `version: <tag without leading v>`
- `release: 1`
- `revision: 0`
- `summary: "Prebyte templating CLI"`
- `description: "Recursive-descent text templating CLI for Prebyte templates."`
- `license: "MIT"`
- `architecture: <x86_64 or aarch64>`
- `vendor: "Coditary"`
- `maintainerEmail: "Matographo@gmail.com"`
- `url: <expected GitHub release asset URL for this .rqp>`

Recommended optional fields:

- `homepage: "https://github.com/Coditary/Prebyte"`
- `sourceUrl: "https://github.com/Coditary/Prebyte"`
- `tags: ["cli", "templating", "prebyte"]`
- `binaries: ["prebyte"]`

Payload metadata must use ReqPack's expected shape:

```json
{
  "path": "payload/payload.tar.zst",
  "archive": "tar",
  "compression": "zstd",
  "hashAlgorithm": "sha256",
  "hashFile": "hashes/payload.sha256",
  "sizeCompressed": 0,
  "sizeInstalledExpected": 0
}
```

The builder will replace the size fields with actual computed values.

Architecture mapping:

- CI `x86_64` -> metadata `x86_64`
- CI `aarch64` -> metadata `aarch64`

Platform is encoded in the archive filename and release asset URL, not in the ReqPack architecture field.

## Hook Manifest

`reqpack.lua` inside the package will declare both install and remove hooks:

```lua
return {
  apiVersion = 1,
  hooks = {
    remove = "scripts/remove.lua",
    install = "scripts/install.lua",
  }
}
```

No dedicated `update` hook is added in v1.

## Package Runtime Layout

Installed package files will live under a versioned data root:

```text
~/.local/share/prebyte/<version>-<release>+r<revision>/
  bin/prebyte
  share/doc/prebyte/LICENSE
  share/doc/prebyte/README.md
```

A stable user-facing symlink will be managed at:

```text
~/.local/bin/prebyte
```

That symlink will point to the installed versioned binary.

Example:

```text
~/.local/bin/prebyte -> ~/.local/share/prebyte/1.0.0-1+r0/bin/prebyte
```

This keeps each installed package version isolated while still exposing one stable executable path.

## Install Hook Behavior

`scripts/layout.lua` will provide shared helpers for:

- version identity string
- versioned install root
- docs directory
- stable symlink path
- path joins and shell quoting helpers

`scripts/install.lua` is responsible for copying files from `context.paths.payloadDir` into the versioned install location and updating the stable symlink.

Install behavior:

1. compute package version root under `~/.local/share/prebyte/<version>-<release>+r<revision>`
2. ensure versioned `bin` directory exists
3. ensure versioned `share/doc/prebyte` directory exists
4. ensure `~/.local/bin` exists
5. copy payload files into the versioned location
6. register created directories and copied files with `context.artifacts.register_*`
7. create or replace symlink `~/.local/bin/prebyte` to point to versioned binary
8. register the symlink with `context.artifacts.register_symlink`
9. return success

Files copied:

- `payloadDir/bin/prebyte` -> `~/.local/share/prebyte/<version>-<release>+r<revision>/bin/prebyte`
- `payloadDir/share/doc/prebyte/LICENSE` -> `~/.local/share/prebyte/<version>-<release>+r<revision>/share/doc/prebyte/LICENSE`
- `payloadDir/share/doc/prebyte/README.md` -> `~/.local/share/prebyte/<version>-<release>+r<revision>/share/doc/prebyte/README.md`

Rules for the hook:

- use ReqPack filesystem helpers, not shell commands
- fail explicitly if expected payload files are missing
- keep logic idempotent where possible
- register every installed directory and file so uninstall stays reliable

Symlink behavior:

- install always points `~/.local/bin/prebyte` at the version being installed
- the stable symlink is considered the active executable path

## Remove Hook Behavior

`scripts/remove.lua` will:

1. compute the same versioned install root as `install.lua`
2. compute expected symlink target for this package version
3. remove the stable symlink only if it currently points to this package version
4. allow ReqPack manifest cleanup to remove registered versioned files and directories
5. return success

Rules for remove:

- do not remove a symlink that already points to a newer package version
- do not try to delete other package versions
- stay safe during update flows where old package removal happens before new package install

Update behavior in practice:

- ReqPack update removes old installed package first
- old remove hook removes old version root and removes symlink only if it still points to old version
- new install hook then writes new version files and repoints symlink to new version

This is sufficient for v1 without needing a dedicated `update.lua` hook.

## CI Integration

The existing GitHub Actions matrix remains source of truth for compiled binaries.

ReqPack build behavior:

- only runs on tag builds
- only runs for Linux and macOS jobs
- runs after existing binary packaging step
- writes `.rqp` files into `dist/`

Workflow changes:

1. install `zstd` on Linux and macOS runners used for release packaging
2. add new packaging step that calls `scripts/ci/package_reqpack.py`
3. skip that step on Windows
4. extend artifact upload patterns to include `dist/*.rqp`
5. keep release upload step generic enough to publish `.rqp` alongside existing archives

The current release flow for `.tar.gz`, `.zip`, and Docker images remains unchanged.

## Builder Script Behavior

`scripts/ci/package_reqpack.py` will:

1. accept version, platform, arch, binary path, and output dir
2. normalize version by stripping leading `v`
3. validate that input binary exists
4. read package template files from `packaging/reqpack/package/`
5. create payload install tree
6. tar payload tree into `payload/payload.tar`
7. compress payload with `zstd` into `payload/payload.tar.zst`
8. compute SHA-256 into `hashes/payload.sha256`
9. render `metadata.json` from `metadata.json.in`
10. copy package scripts and hook manifest into control tree
11. tar final package root into `.rqp`
12. validate required top-level entries before returning success

The script should stay close in style and responsibility to the existing `scripts/ci/package_binary.py` helper.

## Validation

Validation for v1:

- local builder sanity check that required `.rqp` structure exists
- CI artifact upload must find `dist/*.rqp`
- Linux/macOS tag builds must complete with extra `.rqp` outputs

Optional later validation:

- install smoke test using ReqPack in temporary install root
- verify resulting `bin/prebyte` exists after package install

That smoke test is intentionally not required for v1 because ReqPack is not yet part of the current CI environment.

## Naming and URL Rules

The expected release asset URL in package metadata will use GitHub Releases deterministic path:

```text
https://github.com/Coditary/Prebyte/releases/download/v<version>/<archive-name>.rqp
```

Example:

```text
https://github.com/Coditary/Prebyte/releases/download/v1.0.0/prebyte-1.0.0-linux-x86_64.rqp
```

This URL becomes valid once the release job publishes the asset.

## Non-Goals

Explicit non-goals for this work:

- building a ReqPack repository index
- serving package repository metadata from GitHub Pages or another host
- Windows ReqPack support
- packaging Prebyte as a library/developer SDK
- refactoring existing release archive packaging beyond what is needed to add `.rqp`

## Risks and Constraints

Known constraints:

- ReqPack currently supports only Linux and macOS for this use case
- `zstd` must be available on release runners
- the install destination convention depends on `context.paths.installRoot`

Known risks:

- ReqPack may expect a slightly different preferred install layout for CLI tools, though `bin/` plus `share/doc/` is conventional and clear
- metadata `url` points to asset path that exists only after release publication

Mitigation:

- keep install layout simple and conventional
- keep builder isolated in one script so later adjustments are cheap
- keep v1 scope small and additive

## Recommendation

Implement v1 as a small additive extension to the existing release process:

- one `.rqp` per Linux/macOS target
- one visible package template directory
- one new CI builder script
- one static ReqPack manifest
- one shared layout helper
- one install hook
- one remove hook
- one updated workflow upload path

This gets Prebyte into ReqPack-native packaging quickly without introducing repository indexing, cross-platform dispatching, or a second release distribution model.
