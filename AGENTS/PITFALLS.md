# Pitfalls

Verified sharp edges (they *will* bite) and the list of unimplemented 9618 features,
split by topic under `AGENTS/PITFALLS/`.

- [Core language](PITFALLS/core-language.md) — OUTPUT/INPUT, arrays and scoping,
  parameter modes, arithmetic/builtins, CONSTANT, CASE, FOR/WHILE, literal escapes.
- [Multi-file builds and standard library](PITFALLS/modules-and-stdlib.md) — INCLUDE,
  MAIN/GLOBAL rules, embedded std.cps, `-no-std`, `-S`.
- [File handling and TYPE system](PITFALLS/files-and-types.md) — file I/O keywords and
  runtime errors, nominal typing, enums, pointers.
- [OVERLOAD, TEMPLATE, OPERATOR](PITFALLS/overload-template-operator.md) — overload
  resolution, template deduction limits, operator overloading rules.
- [Classes](PITFALLS/classes.md) — access, construction, destruction order, copy limits.
- [Exceptions and C interop](PITFALLS/exceptions-and-c-interop.md) — TRY/CATCH/THROW,
  C types, EXTERN "C", INCLUDE_OBJ, Windows quirks.
- [Not implemented](PITFALLS/not-implemented.md) — missing 9618 features and CI notes.
