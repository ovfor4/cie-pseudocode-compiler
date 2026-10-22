% CPSC-KEYWORD-OR(7)

# NAME

cpsc-keyword-or - combine conditions with logical OR

# SYNOPSIS

```text
left OR right
```

# DESCRIPTION

Return BOOLEAN: true when either operand is true. Both operands must be BOOLEAN; numbers and characters are rejected. OR short-circuits: the right operand is evaluated only when the left one is false. OR binds less tightly than AND and comparisons.

# EXAMPLE

```text
OUTPUT (3 < 2) OR (4 > 1)
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-and(7)](cpsc-keyword-and.7.md), [cpsc-keyword-not(7)](cpsc-keyword-not.7.md), [cpsc-keyword-if(7)](cpsc-keyword-if.7.md).
