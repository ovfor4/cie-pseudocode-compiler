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

A PROCEDURE inside CLASS defines a member procedure, called as `Object.Method(arguments)` or `CALL Object.Method(arguments)`. It follows the same parameter rules and modifies the receiver. Within a method, `this` refers directly to the current object; see [cpsc-keyword-class(7)](cpsc-keyword-class.7.md).

Prefix a definition with OVERLOAD to share its name with other explicitly marked definitions having different parameters or return types; a procedure counts as returning VOID. A CALL statement has no expected result type, so a procedure and a function with the same parameters are ambiguous there unless the function has a DEFAULT return. Other calls require exact argument types and, when a result type is required, an exact return type; see [cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md).

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

[cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md), [cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-class(7)](cpsc-keyword-class.7.md), [cpsc-keyword-call(7)](cpsc-keyword-call.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md).
