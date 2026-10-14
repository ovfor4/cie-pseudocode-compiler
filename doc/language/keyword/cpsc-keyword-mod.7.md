% CPSC-KEYWORD-MOD(7)

# NAME

cpsc-keyword-mod - find the integer division remainder

# SYNOPSIS

```text
left MOD right
```

# DESCRIPTION

Both operands must be INTEGER. Return the remainder after division truncated toward zero; a nonzero remainder has the dividend's sign. A zero divisor throws an exception that TRY/CATCH can handle. MOD has the same precedence as multiplication.

Two C integer operands are also accepted, using C++ integer promotions and signed/unsigned conversion rules. A signed result follows truncation toward zero; an unsigned result uses unsigned division. Mixing C and pseudocode operands requires an explicit cast. See [cpsc-keyword-c_type(7)](cpsc-keyword-c_type.7.md).

# EXAMPLE

```text
OUTPUT 7 MOD 2
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-div(7)](cpsc-keyword-div.7.md), [cpsc-keyword-try(7)](cpsc-keyword-try.7.md).
