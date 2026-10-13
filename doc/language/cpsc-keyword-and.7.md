% CPSC-KEYWORD-AND(7)

# NAME

cpsc-keyword-and - combine conditions with logical AND

# SYNOPSIS

```text
left AND right
```

# DESCRIPTION

Return BOOLEAN: true when both operands are true. Both operands are
evaluated; AND does not short-circuit. AND binds more tightly than OR
and less tightly than comparisons. Use parentheses for clarity.

# EXAMPLE

```text
OUTPUT (3 > 2) AND (4 > 1)
```

# SEE ALSO

[cpsc-language-overview(7)](cpsc-language-overview.7.md), [cpsc-keyword-or(7)](cpsc-keyword-or.7.md), [cpsc-keyword-not(7)](cpsc-keyword-not.7.md), [cpsc-keyword-if(7)](cpsc-keyword-if.7.md).

