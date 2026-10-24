% CPSC-KEYWORD-INCLUDE(7)

# NAME

cpsc-keyword-include - import definitions from another source file

# SYNOPSIS

```text
INCLUDE "path.cps"
```

# DESCRIPTION

INCLUDE is a compiler extension, allowed only at top level. Resolve the path relative to the including source's directory. The included file must also be given on the command line; symbolic and hard links identify the same file.

Import functions, procedures, types (including enum values), classes and methods, top-level constants and GLOBAL variables. Imports are transitive. Repeated includes and cycles are harmless: INCLUDE imports symbols rather than copying statements. A file always sees its own definitions. Using an unimported definition produces the usual unknown or undefined name error. Definitions must be unique across all input files, even when they are not imported. Explicit OVERLOAD definitions may share a name across files when their signatures differ, including by return type; callers see only visible overloads. See [cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md).

Imported constants are visible from the start of the including unit. A unit's own constants retain their ordinary declaration-order visibility. Top-level DECLARE variables are entry-program locals and are not exported. Pass all inputs together; there is no separate object-file compilation mode.

# EXAMPLE

`me.cps`:

```text
INCLUDE "lib.cps"
OUTPUT Add(1, 2)
```

`lib.cps`:

```text
FUNCTION Add(a : INTEGER, b : INTEGER) RETURNS INTEGER
    RETURN a + b
ENDFUNCTION
```

Run with `cpsc me.cps lib.cps -r` to print `3`.

# SEE ALSO

[cpsc(1)](../../cpsc.1.md), [cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-global(7)](cpsc-keyword-global.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md).
