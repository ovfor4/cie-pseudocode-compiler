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

A FUNCTION inside CLASS defines a member function, called as `Object.Method(arguments)`. It follows the same parameter and return rules. Within a method, use `this.Method(arguments)` to call another method on the current object; see [cpsc-keyword-class(7)](cpsc-keyword-class.7.md).

As a compiler extension, a top-level `FUNCTION MAIN RETURNS INTEGER` with no parameters supplies the program entry point. MAIN is case insensitive and cannot coexist with top-level executable statements in any input file. Its INTEGER result, truncated to 32 bits, is returned as the process exit status. An uncaught exception prints the generic termination message and returns 1. A PROCEDURE MAIN or a MAIN with parameters or a non-INTEGER result is an error. The usual default INTEGER result type also applies when RETURNS is omitted.

Prefix a definition with OVERLOAD to share its name with other explicitly marked definitions having different parameter signatures. Calls then require exact argument types; see [cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md).

# EXAMPLE

```text
FUNCTION Double(Value : INTEGER) RETURNS INTEGER
    RETURN Value * 2
ENDFUNCTION
OUTPUT Double(4)
```

# SEE ALSO

[cpsc-keyword-template(7)](cpsc-keyword-template.7.md), [cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md), [cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-class(7)](cpsc-keyword-class.7.md), [cpsc-keyword-procedure(7)](cpsc-keyword-procedure.7.md), [cpsc-keyword-call(7)](cpsc-keyword-call.7.md).
