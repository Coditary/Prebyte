# Recursive-Descent Parser Redesign

## Summary

This document defines Phase 1 of the Prebyte rewrite.

Phase 1 replaces the current string-scanning preprocessing engine with a recursive-descent parser, a typed AST, and a focused render pipeline that follows the Single Responsibility Principle.

This phase also switches the primary template syntax to `{{ ... }}` and introduces a testable architecture for CLI parsing, configuration resolution, template parsing, and rendering.

## Goals

Phase 1 must deliver the following:

1. Recursive-descent parser for template input.
2. New default template syntax based on `{{ ... }}`.
3. Clean SRP-oriented module boundaries.
4. Support for file input or stdin.
5. Support for file output or stdout.
6. Variable definitions from CLI and settings files.
7. Rules and settings resolution, including global and file-specific rules.
8. Includes with recursion and cycle protection.
9. Conditional rendering with `if`, `elseif`, `else`, and `endif`.
10. Builtin read-only values such as `__TIME__`, `__LINE__`, and `__FILE__`.
11. Unit tests and integration tests for the new pipeline.

## Non-Goals

The following items are intentionally excluded from Phase 1:

1. General-purpose Lua integration that replaces the native render path.
2. `for` loops.
3. `while` loops.
4. Macros.
5. User-defined function bindings.
6. User-facing caching features.
7. Compatibility with legacy `%% ... %%` template syntax.

These are deferred until the parser, runtime model, and diagnostics are stable.

## Product Semantics

### Input and Output

1. If a positional input path is provided, Prebyte reads from that file.
2. If no input path is provided, Prebyte reads from stdin.
3. If `-o <path>` or `--output <path>` is provided, Prebyte writes to that file.
4. If no output path is provided, Prebyte writes to stdout.

### Variable Definitions

Variables are defined through `-D` or `--define`.

Supported forms:

1. `-DNAME=value`
   Sets variable `NAME` to the literal string `value`.
2. `-DNAME="Value of text"`
   Sets variable `NAME` to the quoted string value passed by the shell.
3. `-DNAME=@path/to/file`
   Reads file content and stores it as variable `NAME`.
4. `-DNAME=@@literal`
   Escapes the file marker and stores `@literal` as the variable value.
5. `-Dpath/to/file`
   Loads variable definitions from the file. Supported file formats are resolved by extension.

Supported variable import file types in Phase 1:

1. `.env`
2. `.ini`
3. `.json`
4. `.yaml`
5. `.yml`
6. `.toml`

### Rules

Rules can be defined globally or for specific file scopes.

Supported CLI forms:

1. `--rule strict_variables=true`
   Applies globally.
2. `--rule .md::strict_variables=true`
   Applies to all `.md` files.
3. `--rule README.md::strict_variables=false`
   Applies to the exact file `README.md`.

Rules can also be loaded from settings files.

### Settings

`-s <path>` or `--settings <path>` loads a settings file.

Settings files may define:

1. `variables`
2. `rules`
3. `file_rules`
4. `profiles`
5. `ignore`

### Ignore

`-i <name>` or `--ignore <name>` marks a variable or named behavior to ignore during execution.

Phase 1 keeps ignore support in the command model and runtime settings, but only uses it where the new architecture has a direct equivalent. Ignore behavior must not silently mutate parser structure.

### Profiles

`-p <name>` or `--profile <name>` applies a named profile from settings.

Profiles may contribute:

1. variables
2. rules
3. file rules
4. ignore entries

### Explain and Listing

Phase 1 keeps user-facing utility commands:

1. `-e` or `--explain`
2. `list rules`
3. `list vars`
4. `list profiles`
5. `list ignore`

These commands do not run through the template parser or renderer.

### Render Arguments

Phase 1 carries forward a minimal `ARGS` concept as render arguments.

1. Extra positional values after the input path are stored as render arguments.
2. If stdin is used, render arguments start after `--`.
3. Native templates access them through `ARGS[index]`.
4. Lua receives the same data as `ARGS` with zero-based numeric indexes.
5. Bare `ARGS` is invalid and must produce a diagnostic.
6. Macro-era array semantics beyond indexed lookup are out of scope until macros return.

### Benchmarking

`--benchmark` measures total execution duration for the current command.

Phase 1 only guarantees timing output plus Lua cache hit and miss counters. Memory profiling is not part of Phase 1.

## CLI Grammar

```ebnf
command         := render_command | list_command | explain_command | help_command | version_command ;
render_command  := [input_path] { option } [ "--" ] { render_arg } ;
list_command    := "list" ("rules" | "vars" | "profiles" | "ignore" | "ignores") ;
explain_command := ("-e" | "--explain") topic ;

option          := output_opt | define_opt | rule_opt | settings_opt
                  | ignore_opt | profile_opt | benchmark_opt | debug_opt ;

render_arg      := path | identifier | string_literal ;

output_opt      := ("-o" | "--output") path ;
define_opt      := ("-D" define_expr) | (("-d" | "--define") define_expr) ;
rule_opt        := ("-r" | "--rule") rule_expr ;
settings_opt    := ("-s" | "--settings") path ;
ignore_opt      := ("-i" | "--ignore") identifier ;
profile_opt     := ("-p" | "--profile") identifier ;
benchmark_opt   := "--benchmark" ;
debug_opt       := "-X" | "--debug" ;
```

## Template Grammar

Phase 1 uses `{{` and `}}` as the default delimiters.

```ebnf
template      := { text | tag } ;
tag           := interpolation | include | if_block | lua_expr | lua_block ;
interpolation := "{{" expr "}}" ;
include       := "{{" "include" string_literal "}}" ;
if_block      := "{{" "if" expr "}}" template
                 { "{{" "elseif" expr "}}" template }
                 [ "{{" "else" "}}" template ]
                 "{{" "endif" "}}" ;
lua_expr      := "{{" "lua" string_literal "}}" ;
lua_block     := "{{" "lua:block" "}}" raw_text "{{" "endlua" "}}" ;
```

## Expression Grammar

```ebnf
expr          := or_expr ;
or_expr       := and_expr { "||" and_expr } ;
and_expr      := equality_expr { "&&" equality_expr } ;
equality_expr := unary_expr [ ( "==" | "!=" ) unary_expr ] ;
unary_expr    := [ "!" ] primary_expr ;
primary_expr  := identifier | string | number | boolean | lua_call | "(" expr ")" ;
lua_call      := "lua" "(" string ")" ;
```

## Rendering Semantics

### Variable Lookup

Variable lookup order is:

1. Explicitly defined variables.
2. Environment variables when `allow_env=true`.
3. `default_variable_value` when set and `strict_variables=false`.
4. Empty string when the variable remains unresolved and `strict_variables=false` with no default value.

If `strict_variables=true`, unresolved variables are errors and `default_variable_value` must not apply.

### Case Sensitivity

If `case_sensitive_variables=false`, both variable definition and lookup normalize names to lowercase.

If `case_sensitive_variables=true`, names are matched exactly.

### Native Truthiness

Native `if` and `elseif` conditions coerce values as follows:

1. booleans use their boolean value.
2. numbers are false only when equal to zero.
3. strings are trimmed and compared case-insensitively.
4. empty string, `false`, `0`, `no`, and `off` are false.
5. all other non-empty strings are true.

### Includes

Includes are only available when `allow_includes=true`.

Include resolution order:

1. Relative to the currently rendered file.
2. Relative to configured `include_path`.

Each included file is rendered through the same pipeline as the parent file.

Include cycles are hard errors and must produce diagnostics that include the include chain.

### Builtins

Phase 1 keeps read-only builtin values, at minimum:

1. `__TIME__`
2. `__LINE__`
3. `__FILE__`
4. `ARGS[index]`

`__LINE__` is derived from node source spans, not from line-scanning heuristics.

### Lua

Lua is an explicit opt-in side engine.

Phase 1.5 behavior:

1. Native rendering remains the default path.
2. Lua only runs when a template explicitly uses `lua` syntax.
3. Plain interpolation such as `{{ name }}` never falls back to Lua.
4. Plain native conditions remain in the native expression engine.
5. Lua is intended for complex project-specific logic, not for basic variable replacement.

Supported Lua forms:

1. `{{ lua "return price * 0.9" }}`
2. `{{ if lua("return user_role == 'admin'") }}`
3. `{{ lua:block }} ... {{ endlua }}`
4. `{{ if lua:block }} ... {{ endlua }}`
5. `{{ elseif lua:block }} ... {{ endlua }}`

The design goal is that Lua feels available where needed, but absent everywhere else.

Default whitelisted Lua helpers are:

1. `upper(value)`
2. `lower(value)`
3. `trim(value)`
4. `starts_with(value, prefix)`
5. `ends_with(value, suffix)`

Lua instruction cap is controlled by `lua_instruction_limit`.

Lua memory cap is controlled by `lua_memory_limit_bytes`.

### Delimiters

Although the new default syntax is `{{ ... }}`, the renderer still resolves delimiters through rules.

Phase 1 behavior:

1. Default delimiter pair is `{{` and `}}`.
2. `variable_prefix` and `variable_suffix` remain advanced settings.
3. Documentation and examples use `{{ ... }}` exclusively.
4. Legacy `%% ... %%` is not supported.

### File-Specific Rules

Rules are resolved per rendered file.

This means:

1. global rules apply everywhere
2. extension-based file rules apply to matching files
3. exact-path rules override broader file rules
4. included files receive their own effective settings

## Architecture

Phase 1 replaces the current `Context`-heavy processor model with focused components.

### Modules

1. `cli/CommandParser`
   Parses argv into a normalized command model.

2. `app/Command`
   Typed representation of execution intent.

3. `app/AppRunner`
   Dispatches commands such as render, help, explain, list rules, and list vars.

4. `config/SettingsLoader`
   Reads structured settings files.

5. `config/ProfileMerger`
   Applies selected profiles onto the base configuration state.

6. `config/RuleResolver`
   Resolves global and file-scoped rules into typed effective settings.

7. `config/VariableDefinitionParser`
   Parses CLI `-D` definitions and import files.

8. `io/InputReader`
   Reads stdin or file content.

9. `io/OutputWriter`
   Writes stdout or file content.

10. `template/lexer/TemplateLexer`
    Converts source text into tokens.

11. `template/parser/TemplateParser`
    Recursive-descent parser that turns tokens into AST nodes.

12. `template/ast/*`
    AST node types for templates and expressions.

13. `runtime/RenderSession`
    Holds runtime state for a single render tree.

14. `runtime/Renderer`
    Walks the template AST and produces output.

15. `runtime/ExpressionEvaluator`
    Evaluates expression AST nodes.

16. `runtime/IncludeResolver`
    Resolves include paths, loads content, and tracks cycle state.

17. `runtime/BuiltinRegistry`
    Supplies read-only builtins.

18. `runtime/ExpressionEngine`
    Interface for alternate expression backends.

19. `runtime/LuaRuntime`
    Owns a sandboxed Lua state and initializes it lazily.

20. `runtime/LuaSandbox`
    Builds the restricted Lua environment.

21. `runtime/LuaChunkCache`
    Caches compiled Lua chunks for reuse.

22. `runtime/LuaDirectiveExecutor`
    Executes Lua expression and block nodes.

23. `runtime/LuaValueBridge`
    Converts values between C++ and Lua.

24. `support/Diagnostic`
    Structured error and warning information.

25. `support/SourceSpan`
    File, line, and column metadata attached to tokens and AST nodes.

### SRP Rules

The redesign must follow these principles:

1. No global mutable execution context.
2. No hidden process termination inside low-level components.
3. No parser logic inside IO classes.
4. No rendering decisions inside CLI parsing.
5. No configuration file parsing inside renderer logic.
6. No untyped `Data` objects beyond configuration boundaries.

`Data` may remain at configuration parsing boundaries, but internal runtime code must use typed structures.

## AST Design

### Template AST

Phase 1 template AST nodes:

1. `DocumentNode`
2. `TextNode`
3. `InterpolationNode`
4. `IncludeNode`
5. `IfNode`
6. `LuaExprNode`
7. `LuaBlockNode`

#### DocumentNode

Owns ordered children.

#### TextNode

Stores literal text exactly as parsed.

#### InterpolationNode

Stores one expression AST.

#### IncludeNode

Stores a string literal path for `include`.

Phase 1 intentionally limits include syntax to string literals such as:

```txt
{{ include "partials/header.md" }}
```

#### IfNode

Stores ordered branches.

Each branch contains:

1. condition expression
2. body node list

The final `else` branch has no condition.

#### LuaExprNode

Stores a Lua string snippet that returns one value.

#### LuaBlockNode

Stores a raw Lua block body used for output generation or predicate evaluation.

### Expression AST

Phase 1 expression AST nodes:

1. `IdentifierExpr`
2. `StringExpr`
3. `NumberExpr`
4. `BoolExpr`
5. `UnaryExpr`
6. `BinaryExpr`
7. `GroupedExpr`
8. `LuaCallExpr`

All AST nodes carry `SourceSpan`.

## Recursive-Descent Parser Shape

The parser must be organized by grammar rule.

Required parser entry points:

1. `parseDocument()`
2. `parseNode()`
3. `parseTag()`
4. `parseInterpolation()`
5. `parseInclude()`
6. `parseIfBlock()`
7. `parseLuaExpr()`
8. `parseLuaBlock()`
9. `parseExpression()`
10. `parseOr()`
11. `parseAnd()`
12. `parseEquality()`
13. `parseUnary()`
14. `parsePrimary()`

This structure is required to preserve readability, testability, and easy extension for future features.

## Data Flow

Phase 1 render flow:

```text
argv
-> CommandParser
-> AppRunner
-> SettingsLoader
-> ProfileMerger
-> VariableDefinitionParser
-> RuleResolver(global)
-> InputReader
-> RenderSession
   -> RuleResolver(for current file)
   -> TemplateLexer(delimiters from effective settings)
   -> TemplateParser
   -> AST
   -> Renderer
      -> Native ExpressionEvaluator
      -> LuaExpressionEngine when explicit Lua nodes exist
      -> BuiltinRegistry
      -> IncludeResolver
         -> recursive render through same pipeline for included file
-> OutputWriter
```

Rules are re-resolved for each file rendered by the session.

`list rules` and `list vars` bypass template parsing and rendering.

## Typed Runtime Models

Phase 1 introduces typed models to replace the broad `Context` object.

Minimum new types:

1. `Command`
2. `EffectiveSettings`
3. `RenderSession`
4. `Diagnostic`
5. `SourceSpan`
6. `LuaRuntime`
7. `LuaChunkCache`

Suggested supporting models:

1. `VariableStore`
2. `RuleSet`
3. `FileRule`
4. `ProfileConfig`
5. `InputSource`
6. `OutputTarget`

## Error Handling Strategy

Phase 1 error handling must be explicit and typed.

Recommended result transport:

1. `std::expected<T, Diagnostic>` for single-error boundaries.
2. `std::expected<T, DiagnosticBag>` where aggregation is useful.

Error classes:

1. CLI errors
2. configuration errors
3. lexer errors
4. parser errors
5. native render errors
6. Lua sandbox errors
7. IO errors

Required properties of diagnostics:

1. stable error code
2. human-readable message
3. file path
4. line and column
5. source snippet when available
6. include chain when relevant

Expected rendering format:

```text
error[PARSE001]: expected 'endif'
  --> templates/main.md:12:5
  12 | {{ else }}
     |     ^ unexpected else without open if
```

Low-level code must not call `exit()`.

CLI entry points may convert diagnostics into process exit codes.

Library entry points must return or throw structured failures without terminating the host process.

## Testing Strategy

Phase 1 introduces both unit tests and integration tests.

### Test Framework

Recommended framework: `doctest`.

Rationale:

1. small dependency surface
2. easy vendoring
3. low build friction for a Make-based project

### Unit Tests

Required unit test groups:

1. `CommandParserTests`
2. `VariableDefinitionParserTests`
3. `RuleResolverTests`
4. `TemplateLexerTests`
5. `TemplateParserTests`
6. `ExpressionEvaluatorTests`
7. `RendererTests`

Coverage expectations:

#### CommandParserTests

1. file input to stdout
2. stdin to stdout
3. stdin to file
4. file to file
5. `-D` forms
6. `--rule` forms
7. `--settings`
8. `list rules`
9. `list vars`
10. invalid CLI cases

#### VariableDefinitionParserTests

1. `name=value`
2. `name=@file`
3. `name=@@literal`
4. file import
5. malformed syntax

#### RuleResolverTests

1. global rules
2. extension rules
3. exact file rules
4. profile merge
5. case sensitivity rules
6. strict and default interaction

#### TemplateLexerTests

1. plain text
2. open and close tags
3. strings
4. operators
5. line and column tracking
6. unclosed tag

#### TemplateParserTests

1. interpolation
2. include
3. `if/elseif/else/endif`
4. expression precedence
5. parser failures

#### ExpressionEvaluatorTests

1. `&&`
2. `||`
3. `!`
4. `==`
5. `!=`
6. parentheses
7. identifier lookup

#### RendererTests

1. variable substitution
2. strict variables
3. default fallback
4. environment fallback
5. includes
6. builtins
7. file-specific rules
8. include cycle errors
9. explicit Lua expression execution
10. native path without Lua initialization
11. Lua sandbox denial cases
12. Lua chunk cache reuse

### Integration Tests

Required integration scenarios:

1. stdin to stdout
2. file to stdout
3. stdin to file
4. file to file
5. settings plus profile plus CLI variable merge
6. extension rule application on included files
7. strict variable failure
8. include disabled failure
9. `list rules` output
10. `list vars` output
11. diagnostics include file, line, and column
12. benchmark flag output without breaking rendering
13. explicit Lua inline expression render
14. explicit Lua block render
15. mixed native and Lua conditions
16. no-Lua template does not initialize Lua runtime

### Fixtures

Fixture layout:

```text
tests/fixtures/<name>/input.*
tests/fixtures/<name>/settings.*
tests/fixtures/<name>/expected.txt
tests/fixtures/<name>/stderr.txt
tests/fixtures/<name>/command.args
```

Not every fixture requires every file, but the structure should remain consistent.

## File Layout

Phase 1 target layout:

```text
src/main/include/app/
src/main/include/cli/
src/main/include/config/
src/main/include/io/
src/main/include/runtime/
src/main/include/template/ast/
src/main/include/template/lexer/
src/main/include/template/parser/
src/main/include/support/

src/main/cpp/app/
src/main/cpp/cli/
src/main/cpp/config/
src/main/cpp/io/
src/main/cpp/runtime/
src/main/cpp/template/ast/
src/main/cpp/template/lexer/
src/main/cpp/template/parser/
src/main/cpp/support/

tests/unit/
tests/integration/
tests/fixtures/
third_party/doctest/
```

## Migration Plan

Implementation order for Phase 1:

1. Add test harness and `make test` target.
2. Add typed support models such as `Command`, `EffectiveSettings`, `Diagnostic`, and `SourceSpan`.
3. Implement new CLI path with `CommandParser` and `AppRunner`.
4. Implement lexer, recursive-descent parser, and AST.
5. Implement renderer, native expression evaluator, builtin registry, and include resolver.
6. Implement minimal-overhead Lua side engine behind explicit syntax only.
7. Implement settings, profile, variable, and rule resolution.
8. Reach feature parity for Phase 1 plus explicit Lua opt-in behavior.
9. Remove or isolate obsolete parts of the old processor pipeline.
10. Update README and CLI help to describe the new syntax and new scope.

Obsolete or heavily reduced legacy areas are expected to include:

1. `processor/Preprocessor`
2. `processor/ProcessingFlow`
3. `processor/Processor`
4. broad use of `Context`

## Compatibility and Breaking Changes

Phase 1 intentionally introduces breaking changes.

### Breaking Changes

1. Legacy `%% ... %%` syntax is removed.
2. Phase 1 no longer guarantees legacy macro behavior.
3. Phase 1 does not carry forward legacy loop behavior.
4. Phase 1 keeps delimiter customization as an advanced setting, but examples and defaults move to `{{ ... }}`.

### Compatibility Guidance

Users must migrate templates to the new syntax.

README and CLI help must explicitly communicate:

1. new default syntax
2. removed legacy syntax support
3. excluded Phase 1 features
4. future direction for Lua and loop support

## Lua Preparation Without Enabling Lua

Lua is included only as an explicit opt-in side engine.

Required constraints:

1. Native rendering remains primary and fastest.
2. Lua runtime is initialized lazily only when explicit Lua syntax is present.
3. Lua must not run for plain interpolation or plain native conditions.
4. Lua environment must only expose whitelisted data and helper functions.
5. Lua must not expose `os`, `io`, `debug`, `package`, `require`, filesystem access, networking, or shell execution.
6. Lua execution must enforce instruction and memory caps.
7. Lua chunks must be compiled once and reused through cache keys.
8. Lua result caching is out of scope; only compiled chunk caching is required.
9. Short-circuit semantics in native boolean expressions must remain native even when Lua calls appear as subexpressions.

Recommended Lua modes:

1. `InlineValue`
2. `Predicate`
3. `BlockValue`

Recommended benchmark coverage:

1. native-only baseline
2. native conditional baseline
3. single Lua inline expression
4. repeated identical Lua snippet to observe cache reuse
5. mixed native and Lua conditional
6. Lua block output
7. include plus Lua

## Acceptance Criteria

Phase 1 is complete when all of the following are true:

1. Template parsing uses a recursive-descent parser.
2. `{{ variable }}` rendering works.
3. `include` works with cycle detection.
4. `if/elseif/else/endif` works with correct expression precedence.
5. Rules resolve globally and per file.
6. Variables resolve from CLI, settings, profiles, and environment according to defined precedence.
7. `strict_variables` and `default_variable_value` interact exactly as specified.
8. File input, stdin input, file output, and stdout output all work.
9. Explicit Lua syntax runs in a sandboxed, lazy, opt-in path.
10. Plain native templates render without Lua runtime initialization.
11. Unit and integration tests cover the required scenarios.
12. The new implementation no longer depends on a global mutable `Context` to execute Phase 1 flows.
