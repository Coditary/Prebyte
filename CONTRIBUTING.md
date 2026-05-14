# Contributing to Prebyte

Thanks for contributing.

## Setup

Requirements:

1. CMake `>= 3.31`
2. C++23 compiler
3. Ninja or Visual Studio build tools
4. Lua development package

Common local configure:

```bash
cmake --preset dev
cmake --build --preset dev
```

## Testing

Run tests before opening PR:

```bash
cmake --build --preset dev --target prebyte_tests
ctest --preset dev
```

Coverage locally:

```bash
cmake --preset coverage
cmake --build --preset coverage --target prebyte_tests
ctest --preset coverage
gcovr --root . --object-directory build-cmake/coverage --filter src/main/cpp --xml-pretty --output build-cmake/coverage/coverage.xml
```

## Change Expectations

1. Keep changes minimal and focused.
2. Follow existing code style and naming.
3. Add or update tests for behavioral changes.
4. Do not mix unrelated refactors with feature or bugfix work.
5. Preserve cross-platform behavior where possible.

## Pull Requests

Before PR:

1. Rebase or merge current target branch.
2. Ensure CI passes.
3. Update docs when behavior, CLI, build, or release flow changes.

PR description should explain:

1. what changed
2. why it changed
3. how it was tested

## Releases

Tagged releases use GitHub Actions.

1. Push tag like `v1.0.0`
2. CI builds binaries for supported targets
3. Release job uploads artifacts to GitHub Releases
4. Docker job publishes Linux multi-arch image to GHCR

## Security and Secrets

1. Never commit credentials, tokens, or private keys.
2. Keep secrets in GitHub Actions secrets or trusted OIDC integrations.
3. Coverage upload uses Codecov action; activate repo in Codecov and set `CODECOV_TOKEN` in repository or organization Actions secrets before expecting badge updates.
