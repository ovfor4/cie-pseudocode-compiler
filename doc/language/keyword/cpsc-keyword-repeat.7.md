% CPSC-KEYWORD-REPEAT(7)

# NAME

cpsc-keyword-repeat - repeat until a condition is true

# SYNOPSIS

```text
REPEAT
    statements
UNTIL condition
```

# DESCRIPTION

Test after each iteration, so the body runs at least once. Stop when the condition becomes true. UNTIL belongs to this syntax.

# EXAMPLE

```text
DECLARE Count : INTEGER
Count <- 0
REPEAT
    Count <- Count + 1
    OUTPUT Count
UNTIL Count = 3
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-while(7)](cpsc-keyword-while.7.md), [cpsc-keyword-for(7)](cpsc-keyword-for.7.md).
