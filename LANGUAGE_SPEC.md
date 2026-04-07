# Prebyte Language Specification

Version: 0.1-draft
Date: 2026-04-07
Status: Proposed

## 1. Purpose

This document defines the proposed syntax and runtime semantics of the Prebyte template language.

It exists to make lexer, parser, evaluator, and renderer implementation possible without keeping language behavior vague.

This is still a draft spec, but unlike `REQUIREMENTS.md`, it is intentionally concrete.

## 2. Design Goals

- Keep the grammar simple enough for a fast lexer and recursive-descent parser.
- Support enough logic for realistic templating and scaffolding.
- Keep evaluation deterministic.
- Avoid arbitrary code execution.
- Preserve predictable whitespace and newline behavior.
- Treat missing data safely by default.

## 3. Core Concepts

The language supports two main template construct categories:

- inline expressions
- block directives

Inline expressions are intended for same-line substitution.

Block directives are intended for multi-line control flow and template structure.

The engine must support configurable delimiters, but this spec uses the default examples below:

- inline expression delimiter: `{{ ... }}`
- block directive delimiter: `{% ... %}`

Note:

- The implementation may also support alternate delimiter pairs such as `%% ... %%` through configuration.
- Even when delimiters are configurable, the semantic distinction between inline expressions and block directives must remain.

## 4. Data Types

The evaluator must support the following value types:

- string
- integer
- float
- boolean
- null
- array
- object
- missing

`missing` is a first-class internal evaluation state used for safe access propagation.

## 5. Variable and Path Access

### 5.1 Identifiers

Identifiers should support:

- ASCII letters
- digits after the first character
- underscore

Example:

- `name`
- `project_name`
- `user1`

### 5.2 Member Access

Object fields are accessed with dot notation.

Example:

```text
{{ project.author.name }}
```

### 5.3 Index Access

Arrays are accessed with bracket notation.

Example:

```text
{{ users[0].name }}
```

### 5.4 Safe Missing Propagation

Missing access must be safe by default.

Required semantics:

- If a root variable does not exist, the result is `missing`.
- If a member is accessed on `missing`, the result remains `missing`.
- If an index is accessed on `missing`, the result remains `missing`.
- If an array index is out of bounds, the result is `missing`.
- If an object key does not exist, the result is `missing`.
- Chained access such as `test[0].hello.name` must not crash evaluation.

Example:

```text
{{ test[0].hello.name }}
```

If `test[0]` does not exist:

- evaluation result is `missing`
- rendering does not hard-fail by default
- strict failure happens only if active rules require it

## 6. Output Semantics

### 6.1 Inline Expression Output

Inline expressions evaluate to a value and write that value into the current output position.

Example:

```text
Hello {{ user.name }}
```

### 6.2 Missing Inline Output

When an inline expression evaluates to `missing`:

- if `strict_variables=false`, it is treated as unresolved output according to active rules
- if `default_variable_value` exists, that fallback may be used
- if `strict_variables=true`, rendering fails

The exact precedence between `default_variable_value` and strict mode must follow the engine rules defined in `REQUIREMENTS.md`.

## 7. Expressions

Inline expressions and directive conditions may use expressions.

### 7.1 Supported Expression Features

- variable reference
- member access
- index access
- literals
- unary operators
- binary operators
- comparison operators
- boolean operators
- parenthesized expressions
- built-in functions or built-in values

### 7.2 Literals

Supported literals:

- strings
- integers
- floats
- booleans: `true`, `false`
- `null`

Examples:

```text
{{ "hello" }}
{{ 42 }}
{{ 3.14 }}
{{ true }}
{{ null }}
```

### 7.3 Operators

Planned operator classes:

- unary: `!`, unary `-`
- arithmetic: `+`, `-`, `*`, `/`, `%`
- comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- boolean: `&&`, `||`

### 7.4 Truthiness

The evaluator should use the following truthiness rules:

- `false` is false
- `null` is false
- `missing` is false
- empty string is false
- empty array is false
- empty object is false
- numeric zero is false
- all other values are true

This rule set keeps control-flow predictable and useful for templating.

## 8. Control Flow

Control flow is expressed with block directives.

### 8.1 If / Else If / Else

Example syntax:

```text
{% if user.is_admin %}
Admin
{% else if user.is_guest %}
Guest
{% else %}
User
{% end %}
```

Rules:

- `if` starts a conditional block
- `else if` adds an additional conditional branch
- `else` provides the fallback branch
- `end` closes the block

### 8.2 For Loops

Example syntax:

```text
{% for item in items %}
- {{ item.name }}
{% end %}
```

Rules:

- `for` iterates arrays or other iterable values supported by the engine
- the loop body may contain nested expressions and nested directives
- empty collections produce no loop body output

### 8.3 While Loops

Example syntax:

```text
{% while has_more %}
{{ next_value }}
{% end %}
```

Rules:

- `while` is allowed by language design
- implementation must prevent unsafe infinite-loop behavior

Constraint:

- v1 should define an engine setting for maximum loop iterations or equivalent safety guard

### 8.4 Nesting

All control-flow blocks may be nested.

Example:

```text
{% for user in users %}
{% if user.active %}
{{ user.name }}
{% end %}
{% end %}
```

## 9. Inline vs Block Behavior

The language must distinguish same-line substitution from multi-line structural rendering.

### 9.1 Inline Mode

Inline mode is intended for substitutions that remain on the same line.

Example:

```text
status={{ if enabled }}on{{ else }}off{{ end }}
```

Semantic rule:

- inline constructs must not unexpectedly insert leading newlines

### 9.2 Block Mode

Block mode is intended for directives that control whole regions of output.

Example:

```text
{% if enabled %}
feature = true
{% end %}
```

Semantic rules:

- block constructs may emit multi-line output
- surrounding newline handling must be predictable
- omitted blocks must not leave broken syntax artifacts unless the template author explicitly wrote them

Note:

- The final inline shorthand syntax may still change, but the semantic distinction itself is required.

## 10. Includes

Includes insert another template into the current render flow.

### 10.1 Basic Include

Example syntax:

```text
{% include "partials/header.txt" %}
```

Behavior:

- the included file is loaded from the local filesystem
- the included file is lexed, parsed, and rendered with the same engine
- included content may itself contain expressions, control flow, and includes

### 10.2 Include Resolution

Resolution order should be:

1. current file-relative path, if allowed by engine rules
2. configured `include_path`, if present
3. fail with include-resolution error

### 10.3 Include Safety

Requirements:

- local files only in v1
- no network sources
- include cycles must be detected
- cycle errors must report the include chain clearly

## 11. Built-Ins

Built-ins are reserved names or callable helpers exposed by the engine.

### 11.1 Built-In Namespace

To avoid collisions, built-ins should use a reserved namespace.

Recommended direction:

- `__TIME__`
- `__DATE__`
- `__FILE__`
- `__DIR__`
- `__INDEX__`

or a namespaced function style such as:

- `sys.time()`
- `sys.date()`
- `file.path()`

The final naming convention still needs to be frozen.

### 11.2 Built-In Categories

Planned built-in categories:

- time/date
- input path metadata
- current file metadata
- current directory metadata
- loop metadata
- render metadata

### 11.3 Built-In Usage

Built-ins may be used inside:

- inline output expressions
- conditions
- loops
- include arguments, if supported by the final grammar

## 12. Comments

Template comments are controlled by the `allow_comments` rule.

Proposed syntax:

```text
{# this is a comment #}
```

Behavior:

- comments produce no output
- comments may appear between text and directives

## 13. Whitespace and Newline Handling

Whitespace behavior must be predictable because the engine is intended for code generation and document rendering.

Relevant rule properties:

- `trim`
- `strip`
- `trim_spaces`
- `replace_tabs`
- `tab_size`

Current rule:

- the exact whitespace policy is not fully frozen yet
- the engine must keep whitespace behavior deterministic
- block omission must not produce surprising duplicated blank lines unless implied by the template text itself

## 14. Error Semantics

### 14.1 Parse Errors

Parse errors must include:

- file context where available
- line number
- column number
- a useful message

### 14.2 Evaluation Errors

Evaluation errors include:

- strict missing-variable failures
- invalid operator usage
- invalid loop input types
- include resolution failures
- include cycle failures

### 14.3 Safe Missing Access vs Errors

Safe missing access is not itself an evaluation error.

It only becomes an error when:

- strict variable rules require failure
- a specific construct explicitly requires a resolved value

## 15. Example Snippets

### 15.1 Simple Variable

```text
Hello {{ name }}
```

### 15.2 Nested Access

```text
Author: {{ project.author.name }}
```

### 15.3 Safe Missing Access

```text
Hello {{ users[0].profile.name }}
```

If `users[0]` is missing, the whole expression evaluates to `missing`.

### 15.4 Conditional Block

```text
{% if project.private %}
visibility = private
{% else %}
visibility = public
{% end %}
```

### 15.5 Loop

```text
{% for file in files %}
- {{ file.name }}
{% end %}
```

### 15.6 Include

```text
{% include "partials/footer.txt" %}
```

### 15.7 Built-In

```text
Generated at {{ __TIME__ }}
```

## 16. Open Spec Points

- Final delimiter strategy for expression vs block constructs
- Final shorthand syntax for inline conditionals
- Final built-in naming scheme
- Final structured external variable input format
- Exact whitespace-control semantics
- Exact loop safety configuration for `while`

## 17. Summary

This spec defines a template language that is:

- expressive enough for real template generation
- safe by default for missing nested access
- deterministic in evaluation
- compatible with a simple lexer and recursive-descent parser
- performance-oriented by design
