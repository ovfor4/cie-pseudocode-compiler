% CPSC-KEYWORD-EXTERN(7)

# NAME

cpsc-keyword-extern, cpsc-keyword-extern_function, cpsc-keyword-extern_procedure - declare a C function

# SYNOPSIS

```text
EXTERN "C" EXTERN_FUNCTION name(type, ...) RETURNS type
EXTERN "C" EXTERN_PROCEDURE name(type, ...)
```

# DESCRIPTION

EXTERN is a compiler extension, allowed only at top level. It declares a C function without a body; there is no ENDFUNCTION or ENDPROCEDURE. The name is the exact C symbol; calls from pseudocode remain case insensitive. Call an EXTERN_FUNCTION in an expression and an EXTERN_PROCEDURE with CALL.

Parameters list only types. Parameter and result types must be C types: C_INT, C_CHAR, C_STRING, C_DOUBLE or a C_TYPE alias, including pointers. void is not allowed; use EXTERN_PROCEDURE for a C function returning void. EXTERN_FUNCTION requires RETURNS and EXTERN_PROCEDURE does not accept it. Structs, BYREF and BYVAL are unsupported; declare a C pointer parameter and pass ^Variable instead. Pseudocode values must be cast, as for other C values.

A final `...` makes the function variadic. Variadic arguments must also be C values; they receive C default argument promotions (float to double, narrow integers and _Bool to int or unsigned int).

Every EXTERN symbol must be defined by an object named with INCLUDE_OBJ, otherwise compilation fails. EXTERN cannot be combined with OVERLOAD, and a name may be declared once across all input files; put shared declarations in one file and INCLUDE it.

# EXAMPLE

```text
INCLUDE_OBJ "__libc"
EXTERN "C" EXTERN_FUNCTION printf(C_STRING, ...) RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION abs(C_INT) RETURNS C_INT
DECLARE Count : C_INT
Count <- printf((C_STRING)"%d|", abs((C_INT)-5))
OUTPUT ""
```

# SEE ALSO

[cpsc-keyword-include_obj(7)](cpsc-keyword-include_obj.7.md), [cpsc-keyword-c_type(7)](cpsc-keyword-c_type.7.md), [cpsc-keyword-call(7)](cpsc-keyword-call.7.md), [cpsc-language-overview(7)](../cpsc-language-overview.7.md).
