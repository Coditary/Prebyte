# Issue Backlog

The following items are written so they can be turned into GitHub issues later with minimal rewriting.

## 1. Specify the template language grammar

Define the concrete grammar for variables, expressions, control flow, includes, comments, inline mode, and block mode so lexer and parser work can start on a stable spec.

## 2. Specify CLI behavior, inspection commands, and exit codes

Document flags, positional input handling, stdout/stderr behavior, inspection-only commands, and the exit-code matrix.

## 3. Create the base project structure for CLI, engine, parser, evaluator, and config

Set up the initial module layout for CLI, lexer, parser, AST/IR, evaluator, renderer, rules, settings, and cache.

## 4. Implement input auto-detection for string, file, directory, and stdin

Implement the positional-input resolution order and stdin fallback behavior.

## 5. Implement output routing for stdout, output file, and output directory

Support `-o` for single-output and directory-output modes, including usage errors for invalid combinations.

## 6. Implement the variable store with deterministic last-write-wins behavior

Create the variable-resolution layer that merges values from multiple CLI sources in argument order.

## 7. Implement parsing for `-Dkey=value`

Support direct scalar variable injection from the CLI.

## 8. Implement parsing for `-Dkey=@file` and `@@` escaping

Load variable values from files and treat `@@` as a literal `@` escape sequence.

## 9. Implement parsing for `-d <file>` multi-variable input files

Support reading multiple `key=value` pairs from a variable-definition file.

## 10. Specify and implement external structured variable input

Define how arrays, objects, and nested values are passed in from CLI and variable files, then implement that behavior.

## 11. Implement the rule model and parser for global rules

Add support for `-r key=value`, validation, and normalized internal rule representation.

## 12. Implement selector rules for file extensions and exact relative paths

Support `.md::key=value` and `README.md::key=value` forms.

## 13. Implement deterministic rule precedence resolution

Apply exact-path-over-extension-over-global precedence, with later definitions winning at equal specificity.

## 14. Define and implement JSON rule import for `-r rules.json`

Create the JSON schema for rule files and merge imported rules into the same precedence pipeline as CLI rules.

## 15. Implement the settings namespace for `-s` / `--settings`

Separate parser/engine/cache/CLI settings from template rules while preserving deterministic override behavior.

## 16. Implement a simple high-performance lexer

Tokenize template input with low overhead and support the selected delimiter configuration.

## 17. Implement the recursive-descent parser for expressions and control flow

Parse variable output, expressions, `if`, `else if`, `else`, `for`, `while`, and nesting into a stable internal representation.

## 18. Implement expression evaluation for nested variables, member access, and indexing

Support structured data access such as `user.name`, `items[0]`, and deeper nested expressions.

## 19. Implement inline and block rendering semantics

Add rendering behavior that clearly separates same-line substitution from multi-line control-flow output.

## 20. Implement include processing with preprocessing and cycle detection

Allow templates to include local files, preprocess them through the same engine, and fail cleanly on circular includes.

## 21. Implement built-in function-like variables and runtime metadata helpers

Add a reserved built-in namespace for values such as time, date, current file metadata, and loop metadata.

## 22. Implement the core renderer for strings and single files

Build the main rendering pipeline with variable substitution, expression evaluation, control flow, and output encoding.

## 23. Implement directory rendering with relative path preservation

Render full directory trees into a target directory while keeping relative input structure intact.

## 24. Implement in-memory caching for parsed templates and include dependencies

Add cache keys, cache invalidation, and correctness checks for repeated renders of unchanged templates.

## 25. Evaluate and implement optional persistent cache support

Design and, if justified, implement a disk-backed cache for repeated CLI executions.

## 26. Implement diagnostics: `--benchmark`, `--list_rules`, `--explain`, and `-X`

Add benchmark output, rule inspection, setting/rule explanations, and debug logging without corrupting render output.

## 27. Build the automated test matrix for precedence, includes, control flow, and cache correctness

Create tests for input detection, variable precedence, rule precedence, include behavior, control-flow rendering, diagnostics, and cache invalidation.

## 28. Benchmark and optimize hot paths in lexer, parser, evaluator, and renderer

Measure cold and warm performance, identify bottlenecks, and optimize the most important execution paths.
