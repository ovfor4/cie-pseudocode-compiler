% CPSC-KEYWORD-OVERLOAD(7)

# NAME

cpsc-keyword-overload - explicitly overload functions, procedures and constructors

# SYNOPSIS

```text
OVERLOAD FUNCTION name(parameters) RETURNS type
    statements
ENDFUNCTION
OVERLOAD PROCEDURE name(parameters)
    statements
ENDPROCEDURE
CLASS name
    PUBLIC OVERLOAD PROCEDURE NEW(parameters)
        statements
    ENDPROCEDURE
ENDCLASS
```

# DESCRIPTION

OVERLOAD is a compiler extension, absent from the CIE guide. Put it immediately before FUNCTION or PROCEDURE, after PUBLIC or PRIVATE when an access keyword is present. OVERLOAD is reserved. Every definition sharing a case-insensitive name must specify OVERLOAD. Global functions, procedures, class methods and NEW constructors support it; MAIN and built-in functions do not.

Signatures differ by parameter count, parameter types, or array rank. Return types, FUNCTION versus PROCEDURE, BYREF versus BYVAL, and declared array bounds do not distinguish signatures. Overloads may return different types when their parameter signatures differ. Nominal user types remain distinct; equivalent native C type aliases identify the same type.

Calls require an exact match to the arguments' static types. No implicit conversion is used to select an overload, even when only one OVERLOAD definition exists. Array arguments must be whole array variables with matching element types and rank. After selection, ordinary parameter-mode, array-bound and access checks apply. A failure reports `No matching overload for Name(T1, T2)`. Functions without OVERLOAD retain their ordinary argument conversions.

Overloads may be defined in separate input files. A caller sees only its own and included units' definitions. Duplicate parameter signatures fail across all inputs, even when neither unit includes the other. See [cpsc-keyword-include(7)](cpsc-keyword-include.7.md).

Methods use the same rules; fields and methods cannot share a name. Constructor calls select NEW by their explicit arguments. DECLARE without arguments, class-valued fields and array elements select a parameterless NEW; if NEW exists but has no matching parameterless overload, compilation fails. PRIVATE checks apply to the selected method or constructor.

Operator definitions use OVERLOAD FUNCTION OPERATOR followed by the operator symbol or keyword. They support global and member forms with exact operand types; see [cpsc-keyword-operator(7)](cpsc-keyword-operator.7.md).

# EXAMPLE

```text
OVERLOAD FUNCTION Area(Side : INTEGER) RETURNS INTEGER
    RETURN Side * Side
ENDFUNCTION
OVERLOAD FUNCTION Area(Radius : REAL) RETURNS REAL
    RETURN 3.14159 * Radius * Radius
ENDFUNCTION
OUTPUT Area(3)
OUTPUT Area(2.0)
```

# SEE ALSO

[cpsc-keyword-template(7)](cpsc-keyword-template.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md), [cpsc-keyword-procedure(7)](cpsc-keyword-procedure.7.md), [cpsc-keyword-class(7)](cpsc-keyword-class.7.md), [cpsc-keyword-call(7)](cpsc-keyword-call.7.md).
