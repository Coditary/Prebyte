# Prebyte

Prebyte is a text templating tool and C++ library with a recursive-descent parser, typed AST, file-aware rule resolution, and a `{{ ... }}` syntax.

## Phase 1 Status

Implemented now:

1. `{{ variable }}` interpolation
2. `{{ include "file.txt" }}`
3. `{{ if ... }} ... {{ elseif ... }} ... {{ else }} ... {{ endif }}`
4. stdin or file input
5. stdout or file output
6. CLI variables via `-D`
7. settings and profiles
8. global and file-specific rules
9. unit tests and integration tests
10. benchmark runner with persisted history
11. explicit Lua opt-in via `{{ lua ... }}`, `{{ lua:block }}`, `if lua(...)`, and `if lua:block`

Not implemented yet in Phase 1:

1. `for` loops
2. `while` loops
3. macros
4. user-defined functions
5. advanced cache features

## Build

```bash
make
```

Or with CMake + Ninja:

```bash
cmake --preset dev
cmake --build --preset dev
```

## Test

```bash
make test
```

Or with CMake:

```bash
ctest --preset dev
```

## Benchmark

```bash
make benchmark
```

`make benchmark` runs:

1. internal Prebyte benchmark history update
2. cross-engine comparison in `tools/benchmark_compare/`

Or with CMake:

```bash
cmake --build --preset dev-benchmarks
./build-cmake/dev/prebyte_benchmarks
cmake --build --preset dev-compare-benchmark
```

Benchmark history is stored in `tests/benchmarks/history.md`.

## CLI

Basic usage:

```bash
prebyte input.txt
prebyte input.txt -o output.txt
cat input.txt | prebyte
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
2. `-Dname=value` set variable
3. `-Dname=@path/to/file` read file into variable
4. `-Dname=@@literal` escape leading `@`
5. `-Dpath/to/file.env` import variables from file
6. `-r, --rule <rule>` set global or file-specific rule
7. `-s, --settings <file>` load settings file
8. `-i, --ignore <name>` ignore named variable during render
9. `-p, --profile <name>` apply profile
10. `--benchmark` append timing output
11. `-X, --debug` enable debug flag in effective settings

Render args:

1. Extra positional values after input are exposed as `ARGS[index]`
2. For stdin mode, use `--` before args: `prebyte -- foo bar`
3. `ARGS[0]` is first extra value
4. Bare `ARGS` is invalid; use an index

## Template Syntax

Interpolation:

```txt
Hello {{ name }}
```

Include:

```txt
{{ include "partials/header.txt" }}
```

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

Native condition truthiness:

1. `false`, `0`, `no`, `off`, and empty strings are false
2. `true`, `1`, and other non-empty strings are true
3. String checks are trimmed and case-insensitive

Supported expression operators:

1. `!`
2. `&&`
3. `||`
4. `==`
5. `!=`
6. parentheses

Builtins:

1. `__TIME__`
2. `__LINE__`
3. `__FILE__`
4. `ARGS[index]`

Explicit Lua:

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
14. `error_on_false_input`
15. `lua_instruction_limit`
16. `lua_memory_limit_bytes`
17. `debug`

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
2. `rules`
3. `file_rules`
4. `profiles`
5. `ignore`

Useful explain topics:

1. `prebyte -e rule`
2. `prebyte -e ignore`
3. `prebyte -e profile`
4. `prebyte -e truthiness`
5. `prebyte -e lua`
6. `prebyte -e ARGS`

## C++ API

```cpp
#include "PrebyteEngine.h"

int main() {
    prebyte::Prebyte prebyte;
    prebyte.set_variable("name", "Ada");
    prebyte.add_argument("first-extra");
    std::string output = prebyte.process("Hello {{ name }}\n");
}
```

## Architecture

Current implementation is split into focused modules:

1. `cli/` command parsing
2. `config/` settings, profiles, rules, variable imports
3. `io/` input and output
4. `template/lexer/` tokens
5. `template/parser/` recursive-descent parser
6. `template/ast/` typed AST nodes
7. `runtime/` renderer, include resolver, value resolution
8. `support/` diagnostics and spans

Phase-2 preparation hooks already exist for:

1. alternate expression engines
2. reserved loop directives

## Tests And Fixtures

Tests live in:

1. `tests/unit/`
2. `tests/integration/`
3. `tests/fixtures/`

## Breaking Changes

1. legacy `%% ... %%` syntax removed
2. old processor/context pipeline removed
3. macros and loop directives are no longer available in Phase 1

## License

MIT. See `LICENSE`.
