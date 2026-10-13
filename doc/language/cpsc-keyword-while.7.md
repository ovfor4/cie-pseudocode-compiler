% CPSC-KEYWORD-WHILE(7)

# NAME

cpsc-keyword-while - repeat while a condition is true

# SYNOPSIS

```text
WHILE condition DO
    statements
ENDWHILE
```

# DESCRIPTION

Test before each iteration; the body may run zero times.
DO and ENDWHILE belong to this syntax.

# EXAMPLE

```text
DECLARE Count : INTEGER
Count <- 1
WHILE Count <= 3 DO
    OUTPUT Count
    Count <- Count + 1
ENDWHILE
```

# SEE ALSO

[cpsc-language-overview(7)](cpsc-language-overview.7.md), [cpsc-keyword-repeat(7)](cpsc-keyword-repeat.7.md), [cpsc-keyword-for(7)](cpsc-keyword-for.7.md).

