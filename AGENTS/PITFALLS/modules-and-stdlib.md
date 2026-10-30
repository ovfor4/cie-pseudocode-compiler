# Multi-file builds and the standard library

Part of the [pitfalls index](../PITFALLS.md).

- **Multi-file extensions.** INCLUDE imports symbols, never text; every included file
  must also be an input.
  - Duplicate exports across files fail even without visibility, except distinct
    signatures whose definitions all specify OVERLOAD.
  - Only one file may contain top-level code; top-level DECLARE is a main local, not
    an export.
  - Definitions-only builds fail without FUNCTION MAIN returning INTEGER. MAIN
    cannot coexist with top-level code.
  - GLOBAL accepts only the five primitive scalar types and literal initializers;
    arrays, user types and computed initializers are rejected.
  - Own top-level constants remain positional, imported constants do not.
- **Default standard library.** std.cps and its imported STRING and memory libraries are embedded
  and automatically linked.
  - Use -no-std before explicitly supplying a replacement STRING class/library;
    otherwise duplicate definitions fail.
  - Double-quoted literals are raw C_STRING. STRING contexts, OUTPUT and file text
    require a visible STRING class with IMPLICIT NEW(C_STRING), including under -no-std.
  - C_TYPE/INCLUDE/EXTERN syntax strings are parser metadata and do not construct
    STRING.
  - Keyword NEW/DELETE and MALLOC/REALLOC/FREE require memory.cps; -no-std users
    must explicitly import it. Ordinary constructor/destructor methods still work
    without it.
  - -S emits IR only; default-library manual linking needs string.o, memory.o and
    the C++ runtime.
