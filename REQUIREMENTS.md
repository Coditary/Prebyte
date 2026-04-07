# Prebyte Template Engine Requirements

Version: 0.2-draft
Date: 2026-04-07
Status: Draft

## 1. Vision

Prebyte is a high-performance template engine and CLI designed to render:

- inline strings
- single files
- entire directory trees

The product goal is to deliver a faster, non-Python alternative to tools like Cookiecutter while keeping the internal architecture simple, predictable, and performance-oriented.

The core implementation should favor:

- a simple lexer
- a recursive-descent parser
- an efficient intermediate representation
- a fast rendering pipeline
- low-overhead CLI behavior

## 2. Product Goals

- Provide extremely fast template rendering for strings, files, and directories.
- Support a template language that is expressive enough for real scaffolding and text generation.
- Keep CLI behavior deterministic and script-friendly.
- Support scoped rules and engine settings with explicit precedence.
- Avoid Python runtime dependencies completely.
- Make preprocessing, parsing, and rendering efficient enough for repeated CLI and automation usage.

## 3. Non-Goals

- Full Cookiecutter compatibility
- Python hooks or Python-based execution
- GUI or TUI in v1
- Network-based includes or template fetching in v1
- Plugin system in v1
- Arbitrary code execution inside templates

## 4. MoSCoW Prioritization

### Must Have

- Render inline strings, single files, and entire directory trees
- Auto-detect whether positional input is a file, a directory, or a literal string
- Read from `stdin` when no positional input is provided
- Write to `stdout` when no output target is provided
- Support `-o` for file and directory output
- Support variables via `-Dkey=value`
- Support variables via `-Dkey=@file`
- Support variable files via `-d <file>`
- Support `@@` as an escape for literal `@`
- Support rule definitions via `-r` / `--rule`
- Support rule scope at global, file extension, and exact relative path level
- Define deterministic rule precedence
- Keep `-s` / `--settings` separate from template rules
- Implement a simple lexer and a recursive-descent parser
- Support control flow in template expressions and template blocks
- Support nested variables, structs/objects, arrays, indexing, and member access
- Support includes that load and preprocess other files
- Support built-in function-like variables such as time/date/system metadata equivalents
- Add caching to reduce repeated parsing/render overhead
- Prioritize speed across the entire pipeline

### Should Have

- Rule loading from JSON files via `-r rules.json`
- `--benchmark`
- `--list_rules`
- `-e` / `--explain`
- `-X` for debug output
- Good parse/render error messages with file context
- Cache invalidation behavior that is explicit and predictable

### Could Have

- Structured benchmark output, for example JSON
- Rule origin diagnostics per file
- Extended ignore-pattern syntax beyond simple wildcards
- Optional persistent cache on disk in addition to in-memory cache

### Won't Have in v1

- Remote includes
- Python-based template hooks
- Full Cookiecutter compatibility
- Plugin runtime
- Arbitrary embedded scripting languages

## 5. Functional Requirements

### 5.1 Input Detection

The CLI accepts zero or one positional input.

Input resolution order:

1. If no positional input is provided, read from `stdin`.
2. If the positional input points to an existing file, use file mode.
3. If the positional input points to an existing directory, use directory mode.
4. Otherwise, treat the positional input as a literal string.

Acceptance criteria:

- `prebyte` reads from `stdin`.
- `prebyte template.txt` uses file mode if `template.txt` exists.
- `prebyte templates/` uses directory mode if `templates/` exists.
- `prebyte foo/bar.txt` treats `foo/bar.txt` as a literal string if the path does not exist.

### 5.2 Output Behavior

Rules:

1. Without `-o`, string and file input write to `stdout`.
2. With `-o <file>`, string and file input write to the target file.
3. With directory input, `-o <dir>` is required.
4. Directory rendering must preserve relative input structure.

Acceptance criteria:

- `prebyte "Hello {{name}}" -Dname=World` writes to `stdout`.
- `prebyte template.txt -o out.txt` writes to `out.txt`.
- `prebyte templates/ -o out/` renders into `out/` while preserving relative paths.
- `prebyte templates/` without `-o` exits with a usage error.

### 5.3 Variable Sources

Supported forms:

- `-Dkey=value`
- `-Dkey=@value.txt`
- `-d vars.txt`

Semantics:

- `-Dkey=value` sets `key` directly.
- `-Dkey=@value.txt` loads the entire contents of `value.txt` into `key`.
- `-Dkey=@@value.txt` sets the literal value `@value.txt`.
- `-d vars.txt` loads multiple variables from a file.

Assumption for `-d <file>` in v1:

- UTF-8 encoded file
- one `key=value` pair per line
- blank lines allowed
- lines starting with `#` are comments

Variable precedence:

1. Variable definitions are applied in CLI order.
2. For duplicate keys, last write wins.
3. If `allow_env=true`, environment variables may be used as fallback only.
4. Explicit CLI-defined variables override environment values.
5. If a variable still cannot be resolved, `default_variable_value` may apply.
6. If it still cannot be resolved and `strict_variables=true`, rendering fails.

Missing-value evaluation semantics:

- Accessing a missing root variable evaluates to a missing value.
- Accessing a member on a missing value evaluates to a missing value.
- Accessing an array index on a missing value evaluates to a missing value.
- Accessing an out-of-range array index evaluates to a missing value.
- Accessing a missing object key evaluates to a missing value.
- Missing-value propagation continues through chained access, for example `test[0].hello.name`.
- Missing-value propagation becomes an error only when the active rule set requires strict failure.

Acceptance criteria:

- `-Dname=Alice -Dname=Bob` resolves `name` to `Bob`.
- `-Dbody=@body.txt` uses the contents of `body.txt`.
- `-Dbody=@@body.txt` uses the literal string `@body.txt`.
- Accessing `test[0].hello.name` when `test[0]` does not exist evaluates to a missing value instead of crashing evaluation.

### 5.4 Data Model in Templates

Templates must support structured data and nested access.

Required data capabilities:

- scalar values: string, integer, float, boolean, null
- arrays/lists
- structs/objects/maps
- nested data structures
- member access, for example `user.name`
- index access, for example `users[0]`
- nested expressions, for example `users[0].profile.name`

Acceptance criteria:

- A variable tree such as `project.author.name` can be resolved from structured input.
- Array indexing is supported in expressions.
- Nested object and array access works inside control-flow expressions and output expressions.
- Missing nested access such as `test[0].hello.name` is treated as a missing value, not as an immediate hard failure, unless strict mode requires an error.

### 5.5 Template Language

The template language must support both expression output and control flow.

Supported delimiter behavior:

- variable/expression delimiters are configurable through `variable_prefix` and `variable_suffix`
- examples include `{{ ... }}` and `%% ... %%`

The language must support:

- plain variable output
- expressions within delimiters
- conditional logic: `if`, `else if`, `else`
- loops: `for`, `while`
- nested control flow
- nested variable references
- includes
- built-in function-like values or built-in expression functions

Open syntax point:

- The exact concrete grammar is still to be specified, but the engine must support these language features.

### 5.6 Inline vs Block Control Flow

Control flow and preprocessing must support two rendering modes:

- inline mode: replacement stays on the same output line
- block mode: replacement may emit content that starts on a new line or spans multiple lines

Requirements:

- The language must distinguish constructs intended for inline substitution from constructs intended to control multi-line output.
- The render semantics must preserve predictable newline behavior.
- Block replacements must not accidentally collapse or duplicate surrounding newlines.

Open point:

- The exact syntax that differentiates inline constructs from block constructs still needs to be specified.

Acceptance criteria:

- Inline conditions can render alternative values on the same line.
- Block conditions can include or omit full line blocks without corrupting surrounding formatting.
- Loop output in block mode preserves intended line structure.

### 5.7 Includes

Templates must support including other files.

Requirements:

- Includes may load another file or that file's contents into the current render flow.
- Included content must be preprocessed and rendered using the same engine pipeline.
- Includes must respect active rules and relevant file-scoped behavior.
- Includes are disabled unless `allow_includes=true`.
- Include resolution starts from `include_path` if configured.

Security and determinism requirements:

- Includes must be local-file only in v1.
- Include path resolution must be deterministic.
- Recursive include loops must be detected and fail with a clear error.

Acceptance criteria:

- A template can include another file and render the included content.
- Included templates can themselves contain variables and control flow.
- Circular includes fail with a specific error.

### 5.8 Built-In Variables and Built-In Functions

The engine must support built-in function-like values, for example equivalents of `__TIME__`, `__DATE__`, or similar runtime metadata helpers.

Requirements:

- Built-ins must be clearly namespaced or reserved to avoid collisions with user variables.
- Built-ins may expose time/date, file metadata, path metadata, render metadata, or engine metadata.
- Built-ins must be deterministic where possible, or clearly documented when dynamic.
- Built-ins must be usable inside expressions and control flow.

Examples of desired capabilities:

- current timestamp
- current date/time in formatted form
- input path or current file path
- current relative directory
- render iteration metadata in loops

Open point:

- The exact built-in naming scheme and final built-in set remain to be specified.

### 5.9 Rules

Rules are set via `-r` or `--rule`.

Supported forms:

- global: `-r strict_variables=true`
- file extension scope: `-r .md::strict_variables=true`
- exact relative path scope: `-r README.md::strict_variables=true`
- file-based import: `-r rules.json`

Rule precedence for a specific target file:

1. exact relative path
2. file extension
3. global

If two rules have the same specificity, the later declaration wins.

Additional rule:

- For literal string input and `stdin` without file context, only global rules apply.

Acceptance criteria:

- `-r strict_variables=false -r .md::strict_variables=true` makes markdown files use `strict_variables=true`.
- `-r strict_variables=false -r .md::strict_variables=true -r README.md::strict_variables=false` makes only `README.md` use `false`.

### 5.10 Settings

`-s` / `--settings` is reserved for parser, engine, cache, or CLI settings and is separate from template rules.

Requirements:

- Settings use a separate namespace.
- Settings resolve conflicts with last write wins.
- Settings must not implicitly override template rule properties.

### 5.11 Rule and Property Catalog

Core rule properties for v1:

- `strict_variables`
- `case_sensitive_variables`
- `default_variable_value`
- `variable_prefix`
- `variable_suffix`
- `max_variable_length`
- `replace_tabs`
- `tab_size`
- `trim`
- `strip`
- `trim_spaces`
- `allow_includes`
- `include_path`
- `output_encoding`
- `allow_comments`
- `allow_env`

Additional CLI-adjacent functions:

- `-i` / `--ignore`
- `--benchmark`
- `-e` / `--explain`
- `-X`
- `--list_rules`

#### `strict_variables`

- Type: boolean
- Meaning: fail rendering if a variable or expression dependency cannot be resolved after normal missing-value propagation rules have been applied.

#### `case_sensitive_variables`

- Type: boolean
- Meaning: variable lookup is case-sensitive.

#### `default_variable_value`

- Type: string
- Meaning: fallback value when a variable is missing and rendering is allowed to continue.

#### `variable_prefix`

- Type: string
- Meaning: start delimiter for expressions or variable processing.

#### `variable_suffix`

- Type: string
- Meaning: end delimiter for expressions or variable processing.

#### `max_variable_length`

- Type: integer
- Meaning: maximum allowed variable identifier length.

#### `replace_tabs`

- Type: boolean
- Meaning: replace tab characters in output.

#### `tab_size`

- Type: integer
- Meaning: number of spaces used when replacing tabs.

#### `trim`

- Type: boolean
- Meaning: trim leading and trailing whitespace in defined contexts.
- Open point: exact scope and application points still need specification.

#### `strip`

- Type: boolean
- Meaning: strip specific whitespace or line-boundary content according to template semantics.
- Open point: must be clearly distinguished from `trim`.

#### `trim_spaces`

- Type: boolean
- Meaning: remove spaces according to a defined output policy.
- Open point: must clarify whether only literal spaces or broader whitespace are affected.

#### `allow_includes`

- Type: boolean
- Meaning: enable include processing.

#### `include_path`

- Type: path/string
- Meaning: default base path for resolving includes.

#### `output_encoding`

- Type: string
- Meaning: output file encoding, for example `utf-8`.

#### `allow_comments`

- Type: boolean
- Meaning: enable template comment syntax.

#### `allow_env`

- Type: boolean
- Meaning: permit environment variable lookup as fallback.

#### `error_on_false_input`

- Status: open
- Reason: the intended semantics are still underspecified.

### 5.12 Ignore Semantics

`-i` / `--ignore` defines variables or patterns that should not be resolved.

Assumption for v1:

- exact names
- simple `*` wildcards

Behavior:

- ignored variables are not replaced
- ignored variables do not trigger strict-variable failures

### 5.13 Benchmark, Explain, Debug, and Rule Listing

#### `--benchmark`

- Prints runtime and basic statistics.
- Diagnostic output goes to `stderr`.

#### `-e` / `--explain <name>`

- Explains a rule or setting.
- Should include type, meaning, scope, and allowed values.

#### `-X`

- Enables debug output.
- Debug output goes to `stderr`.

#### `--list_rules`

- Prints normalized active rules.
- Should show scope, property, value, and origin.

## 6. Caching Requirements

Caching is a core requirement because the engine is explicitly optimized for speed.

The implementation must support at least in-memory caching for:

- parsed templates
- token streams or equivalent parse inputs when useful
- include resolution metadata when safe
- reusable rule resolution artifacts when safe

Caching requirements:

- Cache behavior must be deterministic.
- Cache invalidation must account for input file changes and included file changes.
- Cached parse results must not leak state across independent render operations.
- String input caching may use the literal input text and relevant settings as cache key components.
- File-based caching must account for file identity and modification changes.

Should-have extension:

- optional persistent cache on disk for repeated CLI runs

Acceptance criteria:

- Repeated rendering of the same template set with unchanged inputs is measurably faster after warm-up.
- Changing an included file invalidates dependent cached render units.
- Cache-enabled renders produce byte-identical output to cold renders.

## 7. Non-Functional Requirements

### 7.1 Performance

The engine must be built for efficiency first.

Initial target metrics for a measurable first version:

- `--help` in release builds should typically complete under 100 ms.
- String rendering of roughly 10 KB with 100 variables should complete in low single-digit milliseconds inside the engine.
- Single-file rendering of roughly 1 MB should complete well under 100 ms in normal cases.
- Directory rendering of roughly 100 files totaling 10 MB should complete under 1.5 s locally in normal cases.
- Warm-cache repeated runs should outperform cold-cache repeated runs in benchmark mode.

Note:

- Exact benchmark methodology and reference hardware still need to be specified.

### 7.2 Architecture

- The lexer must stay intentionally simple.
- The parser must be implemented as a recursive-descent parser.
- CLI, parser, evaluator, rule resolution, and renderer should be clearly separated.
- The internal representation should optimize the hot render path.
- Control-flow and expression evaluation must avoid arbitrary runtime execution.

### 7.3 Robustness

- Equal inputs with equal rules and equal settings must produce deterministic output.
- Parse and render failures must be clear and actionable.
- Errors should include file path and line/column when relevant.
- Circular include detection must be explicit.
- Invalid control-flow syntax must fail fast.
- Safe missing-value propagation must be deterministic and must not crash chained evaluation.

### 7.4 Security

- No arbitrary code execution in templates
- No network access in v1
- Environment variable access disabled by default
- Includes disabled by default
- Include resolution restricted to local filesystem paths
- Built-ins must not expose sensitive process information by default

### 7.5 Portability

- Linux: must support
- macOS: should support
- Windows: should support
- No Python runtime dependency

## 8. CLI Semantics and Exit Codes

### stdout / stderr

- Render output goes to `stdout` unless a target file or directory is used.
- Errors go to `stderr`.
- Debug and benchmark output go to `stderr`.
- `--list_rules` and `--explain` should behave as inspection commands and must not corrupt render output behavior.

### Exit Codes

- `0`: success
- `2`: CLI or usage error
- `3`: I/O error
- `4`: parse or render error
- `5`: invalid rule or setting configuration
- `6`: include resolution or include cycle error
- `7`: cache consistency or cache configuration error

## 9. Assumptions and Open Points

- The exact concrete grammar for variables, blocks, includes, comments, and expressions is still open.
- The syntax distinction between inline and block control flow is still open.
- The JSON schema for `-r rules.json` is still open.
- The exact behavior of `trim`, `strip`, and `trim_spaces` still needs clarification.
- The meaning of `error_on_false_input` is still open.
- Symlink behavior during directory rendering and include resolution is still open.
- The exact built-in naming convention and built-in catalog are still open.
- The structured input format for complex variables from CLI/files may need an additional formal spec if objects and arrays are passed externally.

## 10. Definition of Done for v1

- All Must-Have requirements are implemented.
- Rule precedence and variable precedence are covered by automated tests.
- Structured data access, control flow, and include behavior are covered by automated tests.
- stdout/stderr behavior is stable and documented.
- Circular include handling is tested.
- Cache correctness and cache invalidation are tested.
- CLI help documents all relevant flags and examples.
- Key performance targets are measurable and benchmarked.
- No open critical defects remain in string, file, or directory rendering paths.
