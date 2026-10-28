% CPSC-KEYWORD-DEFAULT(7)

# NAME

cpsc-keyword-default - select a default return type for an overload

# SYNOPSIS

```text
OVERLOAD FUNCTION name(parameters) RETURNS DEFAULT type
    statements
ENDFUNCTION
```

# DESCRIPTION

DEFAULT is a reserved compiler extension, absent from the CIE guide. Place it between RETURNS and the result type on a non-VOID OVERLOAD FUNCTION. Global functions, class methods and operator functions support it. Templates, including methods of class templates, do not support DEFAULT returns.

When the surrounding context requires a result type, selection still requires that exact return type. Without a required result type, a unique parameter match is selected normally; if matches differ only by return type, the DEFAULT version is selected. Without a default, such a call remains ambiguous. A cast first prefers its target return type, then may select the default and convert its result. DEFAULT does not resolve competing parameter signatures or global/member operator ties.

Each case-insensitive name and canonical parameter signature may have at most one DEFAULT return, including across separate input files. Parameter count, types and array rank distinguish these groups; BYREF/BYVAL and array bounds do not. Different parameter signatures may each have a default. Defaults participate only when visible to the caller, and ordinary access checks still apply after selection.

The standard library marks STR_TO_NUM's REAL results and NUM_TO_STR's STRING results as defaults. Their other return types are selected by context.

# EXAMPLE

```text
OVERLOAD FUNCTION Score(Value : INTEGER) RETURNS DEFAULT REAL
    RETURN 2.5
ENDFUNCTION
OVERLOAD FUNCTION Score(Value : INTEGER) RETURNS INTEGER
    RETURN 3
ENDFUNCTION
OUTPUT Score(1)               // 2.5
DECLARE Number : INTEGER
Number <- Score(1)
OUTPUT Number                 // 3
```

# SEE ALSO

[cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md), [cpsc-keyword-operator(7)](cpsc-keyword-operator.7.md), [cpsc-function-str_to_num(7)](../function/cpsc-function-str_to_num.7.md), [cpsc-function-num_to_str(7)](../function/cpsc-function-num_to_str.7.md).
