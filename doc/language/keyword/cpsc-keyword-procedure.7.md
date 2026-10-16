% CPSC-KEYWORD-PROCEDURE(7)

# NAME

cpsc-keyword-procedure, cpsc-keyword-endprocedure - define a procedure and parameter modes

# SYNOPSIS

```text
PROCEDURE name(parameters)
    statements
ENDPROCEDURE
```

# DESCRIPTION

Invoke with CALL. Parameters use `name : type`, optionally prefixed by BYVAL or BYREF. The default is BYVAL (copy); BYREF modifies the caller's variable and requires an exact type match. Scalar BYREF arguments must be plain variables. A mode remains in effect for following parameters until another mode is specified.

Arrays accept `ARRAY OF type` or `ARRAY[lower:upper, ...] OF type`; explicit bounds must be integer literals. BYVAL copies the array; BYREF shares it. Bare RETURN can exit a procedure before a block terminator; RETURN with a value is invalid. BYVAL, BYREF, RETURN, and ENDPROCEDURE belong here. Calls may precede top-level definitions.

# EXAMPLE

```text
PROCEDURE Increment(BYREF Value : INTEGER)
    Value <- Value + 1
ENDPROCEDURE
DECLARE Count : INTEGER
CALL Increment(Count)
OUTPUT Count
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-call(7)](cpsc-keyword-call.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md).
