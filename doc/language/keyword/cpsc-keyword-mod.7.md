% CPSC-KEYWORD-MOD(7)

# NAME

cpsc-keyword-mod - find the integer division remainder

# SYNOPSIS

```text
left MOD right
```

# DESCRIPTION

Both operands must be INTEGER. Return the remainder after division truncated
toward zero; a nonzero remainder has the dividend's sign.
A zero divisor terminates execution. MOD has the same precedence as
multiplication.

# EXAMPLE

```text
OUTPUT 7 MOD 2
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-div(7)](cpsc-keyword-div.7.md).

