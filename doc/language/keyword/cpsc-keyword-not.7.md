% CPSC-KEYWORD-NOT(7)

# NAME

cpsc-keyword-not - negate a condition

# SYNOPSIS

```text
NOT expression
```

# DESCRIPTION

Return BOOLEAN with the opposite truth value. The operand must be BOOLEAN. NOT is a unary operator; parenthesize comparisons, for example `NOT (Count = 0)`.

# EXAMPLE

```text
OUTPUT NOT (3 = 2)
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-and(7)](cpsc-keyword-and.7.md), [cpsc-keyword-or(7)](cpsc-keyword-or.7.md), [cpsc-keyword-if(7)](cpsc-keyword-if.7.md).
