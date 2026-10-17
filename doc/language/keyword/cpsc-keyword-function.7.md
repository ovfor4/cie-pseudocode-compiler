% CPSC-KEYWORD-FUNCTION(7)

# NAME

cpsc-keyword-function, cpsc-keyword-endfunction - define a function that returns a value

# SYNOPSIS

```text
FUNCTION name(parameters) RETURNS type
    statements
    RETURN expression
ENDFUNCTION
```

# DESCRIPTION

Call a function in an expression as `name(arguments)`. A call may precede its top-level definition. RETURNS specifies the result type; omitting it defaults to INTEGER. RETURN supplies a value of that type. Whole-array results are unsupported.

As a compiler extension, `name(arguments)` may also be a complete statement that discards the result. This differs from guide section 8.2, which requires function calls to occur within expressions. Built-in functions support the same form.

Parameters use `name : type`, optionally prefixed by BYVAL. BYVAL is the default; BYREF is forbidden for functions (guide section 8.3). Array parameters accept `ARRAY OF type` (one dimension) or `ARRAY[lower:upper, ...] OF type` (integer literal bounds). BYVAL arrays are copied. RETURNS, RETURN, BYVAL, and ENDFUNCTION belong here.

# EXAMPLE

```text
FUNCTION Double(Value : INTEGER) RETURNS INTEGER
    RETURN Value * 2
ENDFUNCTION
OUTPUT Double(4)
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-procedure(7)](cpsc-keyword-procedure.7.md), [cpsc-keyword-call(7)](cpsc-keyword-call.7.md).
