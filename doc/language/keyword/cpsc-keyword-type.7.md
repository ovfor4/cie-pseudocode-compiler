% CPSC-KEYWORD-TYPE(7)

# NAME

cpsc-keyword-type, cpsc-keyword-endtype - define enumerations, pointers, and records

# SYNOPSIS

```text
TYPE name = (value, ...)
TYPE name = ^type
TYPE name = C_TYPE("C type expression")
TYPE name
    DECLARE field : type
ENDTYPE
```

# DESCRIPTION

Define types at top level. Enumeration values have globally reserved names; OUTPUT prints their zero-based ordinals. Enumerations support comparisons with the same type and addition/subtraction of INTEGER offsets; invalid ordinals throw exceptions that TRY/CATCH can handle.

A prefix caret takes an address (`^variable`); a suffix caret dereferences a pointer (`pointer^`). Pointee types must match exactly, even where guide section 4.2's example mixes types. Pointers start null; dereferencing null throws an exception. There is no NULL literal. Avoid pointers to local variables after the function returns.

Records contain one or more DECLARE fields and end with ENDTYPE. Access fields with a dot. Whole records copy by value; array fields are unsupported. Distinct user-defined type names remain distinct types. SET/DEFINE are reserved but unsupported.

C_TYPE imports a native C type as an alias. Unlike the original enumerations, records and pointer declarations, C aliases with the same underlying C type are interchangeable. See [cpsc-keyword-c_type(7)](cpsc-keyword-c_type.7.md) for supported types and explicit casts.

# EXAMPLE

```text
TYPE Point
    DECLARE X : INTEGER
    DECLARE Y : INTEGER
ENDTYPE
TYPE PointPtr = ^Point
DECLARE Position : Point
DECLARE Pointer : PointPtr
Pointer <- ^Position
Pointer^.X <- 3
OUTPUT Position.X
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md), [cpsc-keyword-class(7)](cpsc-keyword-class.7.md), [cpsc-keyword-try(7)](cpsc-keyword-try.7.md).
