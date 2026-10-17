% CPSC-KEYWORD-CALL(7)

# NAME

cpsc-keyword-call - invoke a procedure

# SYNOPSIS

```text
CALL name(arguments)
```

# DESCRIPTION

Invoke a previously declared procedure with its parameters. Empty argument lists may be written `CALL name()` or `CALL name`. Use function calls in expressions to obtain their results. As compiler extensions, CALL also accepts functions and discards their results, and `name(arguments)` may stand alone without CALL for either a function or a procedure. Built-ins use the same argument checks in expressions and statements.

# EXAMPLE

```text
PROCEDURE Hello()
    OUTPUT "Hello"
ENDPROCEDURE
CALL Hello()
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-procedure(7)](cpsc-keyword-procedure.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md).
