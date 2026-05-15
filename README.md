# Prebyte

[![Tests](https://img.shields.io/github/actions/workflow/status/Coditary/Prebyte/ci.yml?branch=main&event=push&label=Tests)](https://github.com/Coditary/Prebyte/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/tag/Coditary/Prebyte?label=Release)](https://github.com/Coditary/Prebyte/releases)
[![Coverage](https://img.shields.io/codecov/c/github/Coditary/Prebyte?label=Coverage)](https://app.codecov.io/gh/Coditary/Prebyte)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Prebyte is text templating CLI and C++ library with recursive-descent parser, typed AST, compiled template cache, file-aware include resolution, and `{{ ... }}` syntax.

Contribution guide: see [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Status

Implemented now:

1. `{{ variable }}` interpolation
2. object member access like `{{ user.name }}`
3. list index access like `{{ items[0] }}`
4. `{{ include "file.txt" }}`
5. `{{ if ... }} ... {{ elseif ... }} ... {{ else }} ... {{ endif }}`
6. `{{ for item in items }} ... {{ else }} ... {{ endfor }}`
7. `{{ for key, value in object }} ... {{ endfor }}`
8. `{{ set name = expression }}`
9. filters with pipes: `trim`, `upper`, `lower`, `default`, `replace`
10. builtin variables like `__FILE__`, `__DATE__`, `__UUID__`, `__RANDOM__`
11. explicit Lua via `{{ lua ... }}`, `{{ lua:block }}`, `if lua(...)`, and `if lua:block`
12. user-defined functions via `{{ fn ... }} ... {{ endfn }}` and `{{ fn ... lua:block }} ... {{ endfn }}`
13. stdin or file input
14. stdout or file output
15. CLI variables via `-D`
16. structured variable imports from JSON, YAML, TOML, and `.env`
17. settings and profiles
18. global and file-specific rules
19. unit tests and integration tests
20. benchmark runner with persisted history

Not implemented yet:

1. `while` loops
2. macros separate from functions
3. advanced cache controls exposed to users

## Build

`make` is convenience wrapper for Unix-like shells. Cross-platform path is CMake presets.

```bash
make
```

Or with CMake + Ninja:

```bash
cmake --preset dev
cmake --build --preset dev
```

On Windows, use CMake preset commands above from Developer PowerShell or terminal with CMake, Ninja, compiler, and Lua installed.

Build current-host ReqPack package on Linux or macOS:

```bash
make reqpack
```

Output is written to `dist/`, including target `.rqp` and `index.json`.

## Releases

Tagged releases publish versioned binaries to [GitHub Releases](https://github.com/Coditary/Prebyte/releases).
Linux/macOS releases also publish ReqPack `.rqp` assets plus `index.json` for ReqPack repository consumption.

Container images are published to GHCR for Linux `x86_64` and `aarch64`:

```bash
docker pull ghcr.io/coditary/prebyte:latest
docker run --rm ghcr.io/coditary/prebyte:latest -h
cat input.txt | docker run --rm -i ghcr.io/coditary/prebyte:latest
```

## Test

```bash
make test
```

Or with CMake:

```bash
ctest --preset dev
```

On Windows, prefer `cmake --build --preset dev --target prebyte_tests` then `ctest --preset dev`.

## Benchmark

```bash
make benchmark
```

`make benchmark` runs:

1. internal Prebyte benchmark history update in `tests/benchmarks/history.md`
2. Prebyte vs Go comparison from `tools/benchmark_compare/`

Cross-engine comparison prints three modes:

1. `cold`: fresh engine and parse path on each render
2. `warm-execute`: parse and prepare once, then execute again without final output memoization
3. `warm-memoized`: repeat same render after final output memoization is primed

Benchmark history is stored in `tests/benchmarks/history.md`.

## CLI

Basic usage:

```bash
prebyte input.txt
prebyte input.txt -o output.txt
cat input.txt | prebyte
prebyte template.txt arg0 arg1
prebyte -- foo bar
```

Commands:

1. `prebyte <input>` render file to stdout
2. `prebyte <input> -o <output>` render file to file
3. `prebyte -e <topic>` explain topic
4. `prebyte list rules`
5. `prebyte list vars`
6. `prebyte list profiles`
7. `prebyte list ignore`
8. `prebyte -h`
9. `prebyte -v`

Options:

1. `-o, --output <file>` output file
2. `-I, --include-path <dir>` add include root, first match wins
3. `-Dname=value` set variable
4. `-Dname=@path/to/file` read file into variable
5. `-Dname=@@literal` escape leading `@`
6. `-Dpath/to/file.env` import variables from file
7. `-r, --rule <rule>` set global or file-specific rule
8. `-s, --settings <file>` load settings file
9. `-i, --ignore <name>` ignore named variable during render
10. `-p, --profile <name>` apply profile
11. `--benchmark` append timing output
12. `-X, --debug` enable debug flag in effective settings

Render args:

1. Extra positional values after input are exposed as `ARGS[index]`
2. For stdin mode, use `--` before args: `prebyte -- foo bar`
3. `ARGS[0]` is first extra value
4. Bare `ARGS` is invalid; use an index

## Template Syntax

Interpolation:

```txt
Hello {{ name }}
{{ user.name }}
{{ items[1] }}
```

Include:

```txt
{{ include "partials/header.txt" }}
```

Include lookup order per root:

1. `<include>.pbc`
2. `<include>.pbt`
3. `<include>`
4. `<include>/index.pbc`
5. `<include>/index.pbt`
6. `<include>/index`

Include roots are checked in this order:

1. current file directory
2. each CLI `-I/--include-path` in order
3. settings `include_paths` in order
4. legacy `include_path`
5. `~/.local/share/prebyte` on Unix-like systems or `%LOCALAPPDATA%\Prebyte\share` on Windows

First matching root wins.

Conditionals:

```txt
{{ if enabled }}
Enabled
{{ elseif fallback }}
Fallback
{{ else }}
Disabled
{{ endif }}
```

Loops:

```txt
{{ for item in items }}{{ item }}{{ else }}empty{{ endfor }}
{{ for key, value in user }}{{ key }}={{ value }};{{ endfor }}
```

Set:

```txt
{{ set title = name | trim | upper }}
{{ title }}
```

Whitespace trim:

```txt
A {{- name -}} B
```

Native condition truthiness:

1. `false`, `0`, `no`, `off`, and empty strings are false
2. `true`, `1`, and other non-empty strings are true
3. string checks are trimmed and case-insensitive
4. empty lists and empty objects are false

Supported expression operators:

1. `!`
2. `&&`
3. `||`
4. `==`
5. `!=`
6. `<`, `>`, `<=`, `>=`
7. `in`
8. parentheses

Builtins:

1. `__TIME__`
2. `__LINE__`
3. `__FILE__`
4. `__FILENAME__`
5. `__DIR__`
6. `__EXTENSION__`
7. `__DATE__`
8. `__TIMESTAMP__`
9. `__YEAR__`
10. `__MONTH__`
11. `__DAY__`
12. `__UNIX_EPOCH__`
13. `__USER__`
14. `__HOST__`
15. `__OS__`
16. `__WORKING_DIR__`
17. `__UUID__`
18. `__RANDOM__`
19. `ARGS[index]`

Notes:

1. `__DATE__` uses `YYYY-MM-DD`
2. `__TIMESTAMP__` uses `YYYY-MM-DDTHH:MM:SS`
3. `__EXTENSION__` has no leading dot
4. dynamic builtins like `__UUID__` and `__RANDOM__` stay constant during one render

Filters:

1. `trim`
2. `upper`
3. `lower`
4. `default(fallback)`
5. `replace(from, to)`

Examples:

```txt
{{ name | trim | upper }}
{{ missing | default("fallback") }}
{{ title | replace("_", "-") }}
```

## User Functions

Native template function:

```txt
{{ fn greet(name) }}Hello {{ name }}{{ endfn }}
{{ greet("Ada") }}
```

Lua-backed function:

```txt
{{ fn users() lua:block }}
return { { name = "Ada" }, { name = "Grace" } }
{{ endfn }}

{{ for user in users() }}{{ user.name }};{{ endfor }}
```

Function rules:

1. functions are available only after their definition
2. function definitions do not produce output
3. native template functions return rendered text as string
4. Lua functions can return scalars, lists, objects, booleans, numbers, or null
5. functions defined in a file are visible in later includes from that point
6. functions defined inside an include stay local to that include

## Explicit Lua

```txt
{{ lua "return upper(name)" }}

{{ lua:block }}
return "Hello " .. name
{{ endlua }}

{{ if lua("return enabled == 'true'") }}
Enabled
{{ endif }}

{{ if lua:block }}
return enabled == 'true'
{{ endlua }}
Enabled
{{ endif }}
```

Built-in Lua helpers:

1. `upper(value)`
2. `lower(value)`
3. `trim(value)`
4. `starts_with(value, prefix)`
5. `ends_with(value, suffix)`

Default Lua limits:

1. `lua_instruction_limit=100000`
2. `lua_memory_limit_bytes=4194304`

Lua sandbox blocks `os`, `io`, `debug`, `package`, `require`, `dofile`, and `loadfile`.

## Rules

Examples:

```bash
prebyte input.txt --rule strict_variables=true
prebyte input.txt --rule .md::default_variable_value=Fallback
prebyte input.txt --rule README.md::strict_variables=false
```

Supported rules in current implementation:

1. `strict_variables`
2. `case_sensitive_variables`
3. `default_variable_value`
4. `variable_prefix`
5. `variable_suffix`
6. `max_variable_length`
7. `replace_tabs`
8. `tab_size`
9. `trim`
10. `allow_includes`
11. `include_path`
12. `output_encoding`
13. `allow_env`
14. `forbidden_env_vars`
15. `error_on_false_input`
16. `lua_instruction_limit`
17. `lua_memory_limit_bytes`
18. `max_include_depth`
19. `max_render_time_ms`
20. `max_output_size_bytes`
21. `max_loop_iteration`
22. `debug`

Rule notes:

1. `output_encoding` supports `utf-8` and `utf-16`
2. `output_encoding` only affects file output via `-o/--output` and `Prebyte::process(..., output_path)` / `process_file(..., output_path)`
3. returned strings and stdout output stay UTF-8
4. `error_on_false_input=false` keeps normal `if` / `elseif` fallback behavior
5. `error_on_false_input=true` raises a runtime error when an `if` / `elseif` condition is falsey

## Settings File Shape

Example:

```yaml
variables:
  greeting: Hello

rules:
  strict_variables: false
  lua_memory_limit_bytes: 1048576

file_rules:
  .md:
    default_variable_value: Fallback

profiles:
  friendly:
    variables:
      greeting: Hi
```

Supported top-level keys:

1. `variables`
2. `include_paths`
3. `rules`
4. `file_rules`
5. `profiles`
6. `ignore`

Compiled templates:

1. `.pbt` = source template
2. `.pbc` = compiled template cache
3. Prebyte prefers `.pbc` for includes when cache is still fresh

Useful explain topics:

1. `prebyte -e rule`
2. `prebyte -e ignore`
3. `prebyte -e profile`
4. `prebyte -e truthiness`
5. `prebyte -e lua`
6. `prebyte -e ARGS`

## C++ API

Convenience wrapper:

```cpp
#include "PrebyteEngine.h"

int main() {
    prebyte::Prebyte prebyte;
    prebyte.set_variable("name", "Ada");
    prebyte.add_argument("first-extra");
    std::string output = prebyte.process("Hello {{ name }}\n");
}
```

Embed API:

```cpp
#include "Engine.h"

#include <iostream>

int main() {
    prebyte::Engine engine;
    prebyte::CompiledTemplate tpl = engine.compile("Hello {{ name }}\n");

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");

    std::string output = engine.render(tpl, ctx);

    engine.render_to(tpl, [](std::string_view chunk) {
        std::cout.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    }, ctx);
}
```

Top-level compiled template:

```cpp
#include "Engine.h"

int main() {
    prebyte::Engine engine;
    prebyte::CompiledTemplate tpl = engine.load_compiled_file("template.pbc");

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");

    std::string output = engine.render(tpl, ctx);
}
```

Embed API notes:

1. `render()` is collecting wrapper over `render_to()`
2. `RenderContext` accepts scalar and structured `Value`s
3. `compile_file(path)` uses `path` for both source and logical path defaults
4. sink chunk lifetime is only callback duration
5. stream render may emit partial output before `DiagnosticError`
6. use `load_compiled_file(path)` for top-level `.pbc` templates
7. public API does not expose `.pbc` serialization yet; current `.pbc` files come from CLI/cache/internal serializer paths

## Architecture

Current implementation is split into focused modules:

1. `cli/` command parsing
2. `config/` settings, profiles, rules, variable imports
3. `io/` input and output
4. `template/lexer/` tokens
5. `template/parser/` recursive-descent parser
6. `template/ast/` typed AST nodes
7. `runtime/` renderer, compiled executor, include resolver, value resolution, Lua runtime
8. `support/` diagnostics and spans

## Tests And Fixtures

Tests live in:

1. `tests/unit/`
2. `tests/integration/`
3. `tests/fixtures/`

## License

MIT. See `LICENSE`.
