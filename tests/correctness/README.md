# Correctness

Verifies expected behaviour: parsing, rendering, settings, imports, roundtrips, and CLI-equivalent in-process flows.

- `unit/` — focused module tests (lexer, parser, runtime, config, I/O).
- `integration/` — AppRunner, settings behaviour, structured imports, compiled-template roundtrips.
- `property/` — seeded property tests (compile → serialize → render invariants).

All sources here are linked into `prebyte_tests` and discovered by CTest.
