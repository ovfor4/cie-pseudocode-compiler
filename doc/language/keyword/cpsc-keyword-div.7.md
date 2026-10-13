% CPSC-KEYWORD-DIV(7)

# NAME

cpsc-keyword-div - perform integer division

# SYNOPSIS

```text
left DIV right
```

# DESCRIPTION

Both operands must be INTEGER. Return the quotient truncated toward zero. A zero divisor terminates execution. DIV has the same precedence as multiplication. Use `/` for division returning REAL.

# EXAMPLE

```text
OUTPUT 7 DIV 2
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-mod(7)](cpsc-keyword-mod.7.md).
