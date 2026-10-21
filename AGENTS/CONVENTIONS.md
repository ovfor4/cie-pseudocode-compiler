# Load-bearing conventions

These invariants span modules. Violating any of them breaks code you didn't touch.
(Overview and build instructions: [ARCH/OVERVIEW.md](ARCH/OVERVIEW.md),
[ARCH/BUILD.md](ARCH/BUILD.md).) The details are split by topic under
`AGENTS/CONVENTIONS/`.

- [LEXING.md](CONVENTIONS/LEXING.md) — English-only project text, raw `int` tokens,
  UPPERCASE keywords, identifier rules, `__cps_` reserved prefix, caller-owned lexer
  stream.
- [TYPES.md](CONVENTIONS/TYPES.md) — type names as strings, fixed builtin IR
  representations, structural C aliases, nominal user kinds, global enum namespace.
- [STRINGS.md](CONVENTIONS/STRINGS.md) — STRING as a library class, literal forms,
  and value-copy ownership rules.
- [SYMBOLS_AND_ERRORS.md](CONVENTIONS/SYMBOLS_AND_ERRORS.md) — the single `Symbols`
  table and scoping, designators, `dynamic_cast` dispatch chains, error reporting.
- [CALLS.md](CONVENTIONS/CALLS.md) — argument marshalling, signature-based dispatch,
  parameter modes, array parameters, builtins table.
- [TEMPLATES_AND_OPERATORS.md](CONVENTIONS/TEMPLATES_AND_OPERATORS.md) — template
  substitution and selection, operator and assignment overloads.
- [EXCEPTIONS_AND_UNITS.md](CONVENTIONS/EXCEPTIONS_AND_UNITS.md) — C++ ABI exceptions,
  compilation units, MAIN, library units, default standard library.
