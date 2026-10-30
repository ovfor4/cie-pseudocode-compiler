% CPSC-KEYWORD-CONSTANT(7)

# NAME

cpsc-keyword-constant - declare a named literal value

# SYNOPSIS

```text
CONSTANT identifier = literal
```

# DESCRIPTION

Declare a read-only value whose type is inferred from its literal: INTEGER, REAL, BOOLEAN, CHAR or C_STRING. Both ordinary and C-suffixed double-quoted literals produce C_STRING; STRING contexts use its IMPLICIT constructor. A negative integer or real literal is allowed. As required by the Cambridge guide, variables, other constants, expressions, casts, and function calls cannot initialize a CONSTANT.

A constant is visible from its declaration to the end of its enclosing block. A top-level constant is also available in functions and procedures defined after its declaration. Functions do not inherit constants from another function or from an enclosing statement block.

Inner blocks may shadow outer names; leaving a block restores bindings hidden by constants or hiding constants. Each IF branch, loop body, TRY body, CATCH body, and CASE branch has its own constant scope. Ordinary variables otherwise retain their existing function scope. A declaration in the same scope cannot reuse a constant name, and a constant cannot reuse a variable or parameter name in that scope. Type names and enumerator names remain reserved; top-level constants cannot reuse function names.

Constants may be read in expressions, used as CASE labels or array bounds, and passed BYVAL. Assignment, INPUT, READFILE targets, FOR control variables, BYREF arguments, and taking the address of a constant are rejected.

# EXAMPLE

```text
CONSTANT HourlyRate = 6.50
CONSTANT DefaultText = "N/A"
OUTPUT HourlyRate, DefaultText
IF TRUE THEN
    CONSTANT HourlyRate = 8.00
    OUTPUT HourlyRate
ENDIF
OUTPUT HourlyRate
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md), [cpsc-keyword-case(7)](cpsc-keyword-case.7.md).
