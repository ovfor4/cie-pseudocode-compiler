% CPSC-KEYWORD-OVERLOAD(7)

# NAME

cpsc-keyword-overload - explicitly overload functions, procedures, methods and constructors

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

OVERLOAD is a compiler extension, absent from the CIE guide. Put it before FUNCTION or PROCEDURE, after PUBLIC or PRIVATE when an access keyword is present. An implicit constructor places IMPLICIT between OVERLOAD and PROCEDURE. OVERLOAD is reserved. Every definition sharing a case-insensitive name must specify OVERLOAD. Global functions, procedures, class methods and NEW constructors support it; MAIN and built-in functions do not.

Signatures differ by parameter count, parameter types, array rank, or return type. BYREF versus BYVAL and declared array bounds do not distinguish signatures. A PROCEDURE counts as returning VOID, so it can coexist with a FUNCTION having the same parameter list; a call that cannot determine a result type is then ambiguous unless the function specifies a DEFAULT return. Nominal user types remain distinct; equivalent native C type aliases identify the same type.

Calls prefer exact static parameter types. If no exact candidate matches, BYVAL parameters may use a single IMPLICIT constructor or the existing CHAR/STRING bridge. Competing converted candidates are ambiguous; numeric widening does not select an overload. See [cpsc-keyword-implicit(7)](cpsc-keyword-implicit.7.md). Array arguments must be whole array variables with matching element types and rank. After selection, ordinary parameter-mode, array-bound and access checks apply. Functions without OVERLOAD retain their ordinary argument conversions and their implicit result conversions.

When the context requires a known result type, the selected overload must return exactly that type. This applies to assignment targets, BYVAL arguments, RETURN values, conditions, FOR bounds, subscripts and CASE comparisons. If the arguments match but no overload returns the required type, the error is `No matching overload for Name(T1, T2) returning T`. A cast `(T) expr` is weaker: it first prefers an overload returning T, then uses ordinary parameter selection and converts the result to T.

Without an expected result type (for example OUTPUT, an operand of another operator or comparison, a CASE selector, a CALL statement, or an argument passed to another OVERLOAD call), a unique parameter match is selected. If several candidates share the same parameter list and differ only by return type, select the one marked `RETURNS DEFAULT type`; without a default, the call is ambiguous. Each name and canonical parameter signature may have only one default, including across input files. DEFAULT does not resolve parameter ambiguity or override a required result type; see [cpsc-keyword-default(7)](cpsc-keyword-default.7.md).

Overloads may be defined in separate input files. A caller sees only its own and included units' definitions. Duplicate signatures fail across all inputs, even when neither unit includes the other; two definitions are duplicates only when both their parameters and their return types match. See [cpsc-keyword-include(7)](cpsc-keyword-include.7.md).

Methods use the same rules, including result-type selection; fields and methods cannot share a name. Constructor calls select NEW by their explicit arguments. DECLARE without arguments, class-valued fields and array elements select a parameterless NEW; if NEW exists but has no matching parameterless overload, compilation fails. PRIVATE checks apply to the selected method or constructor.

Operator definitions use OVERLOAD FUNCTION OPERATOR followed by the operator symbol or keyword. They support global and member forms with exact types preferred over implicit construction; see [cpsc-keyword-operator(7)](cpsc-keyword-operator.7.md).

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

[cpsc-keyword-default(7)](cpsc-keyword-default.7.md), [cpsc-keyword-template(7)](cpsc-keyword-template.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md), [cpsc-keyword-procedure(7)](cpsc-keyword-procedure.7.md), [cpsc-keyword-class(7)](cpsc-keyword-class.7.md), [cpsc-keyword-call(7)](cpsc-keyword-call.7.md).
