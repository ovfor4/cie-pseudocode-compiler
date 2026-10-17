% CPSC-KEYWORD-CALL(7)

# NAME

cpsc-keyword-call - invoke a procedure

# SYNOPSIS

```text
CALL name(arguments)
```

# DESCRIPTION

Invoke a previously declared procedure with its parameters. Empty argument lists may be written `CALL name()` or `CALL name`. Use function calls in expressions to obtain their results. As compiler extensions, CALL also accepts functions and discards their results, and `name(arguments)` may stand alone without CALL for either a function or a procedure. Built-ins use the same argument checks in expressions and statements.

Member calls accept `CALL Object.Method(arguments)` or the standalone form `Object.Method(arguments)`. With no arguments, `CALL Object.Method` is also accepted. Inside a method, use `this.Method(arguments)` for the current object; unqualified calls continue to name global functions or procedures. See [cpsc-keyword-class(7)](cpsc-keyword-class.7.md).

# EXAMPLE

```text
PROCEDURE Hello()
    OUTPUT "Hello"
ENDPROCEDURE
CALL Hello()
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-class(7)](cpsc-keyword-class.7.md), [cpsc-keyword-procedure(7)](cpsc-keyword-procedure.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md).
