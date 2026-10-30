% CPSC-KEYWORD-IMPLICIT(7)

# NAME

cpsc-keyword-implicit - allow automatic construction of a class value

# SYNOPSIS

```text
CLASS name
    [PUBLIC|PRIVATE] [OVERLOAD] IMPLICIT PROCEDURE NEW(value : type)
        statements
    ENDPROCEDURE
ENDCLASS
```

# DESCRIPTION

IMPLICIT is a compiler extension, absent from the CIE guide. Place it before PROCEDURE NEW, after OVERLOAD when present. The constructor must take exactly one BYVAL parameter, which may omit the BYVAL keyword. BYREF, array and parameter-pack parameters are rejected. Other methods and global functions cannot be IMPLICIT.

A marked constructor allows its parameter type to become the owning class type in assignment, BYVAL argument passing, RETURN and overload selection. The parameter must match the source type directly; equivalent C aliases count as the same type. Conversion does not chain through another IMPLICIT constructor. Unmarked constructors remain available for explicit construction. No standard-library exception grants an unmarked constructor automatic conversion.

Exact parameter matches take priority. If none matches, functions and operators may use implicit construction; competing candidates at the same level are ambiguous. PRIVATE access is checked after selection. BYREF parameters and whole arrays never convert. Existing result-type and DEFAULT selection rules still apply; see [cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md). Directly supported built-in operations precede converted operators.

Double-quoted literals and their CONSTANT values have type C_STRING. The standard library supplies IMPLICIT STRING constructors for C_STRING and CHAR. String functions, comparisons and concatenation therefore retain their normal text behavior. OUTPUT and file operations also bridge raw text through the marked STRING constructor. The older STRING-to-CHAR conversion checks for exactly one character; raw text can use this bridge after constructing STRING, with lower priority than a direct STRING parameter. The adjacent C suffix remains accepted and has the same C_STRING type.

# EXAMPLE

```text
CLASS Box
    Value : INTEGER
    IMPLICIT PROCEDURE NEW(Number : INTEGER)
        Value <- Number
    ENDPROCEDURE
ENDCLASS
DECLARE Item : Box
Item <- 42
OUTPUT Item.Value
```

# SEE ALSO

[cpsc-keyword-class(7)](cpsc-keyword-class.7.md), [cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md), [cpsc-keyword-operator(7)](cpsc-keyword-operator.7.md), [cpsc-keyword-c_type(7)](cpsc-keyword-c_type.7.md).
