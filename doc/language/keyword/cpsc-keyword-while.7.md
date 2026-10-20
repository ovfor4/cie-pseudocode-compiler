% CPSC-KEYWORD-WHILE(7)

# NAME

cpsc-keyword-while, cpsc-keyword-endwhile - repeat while a condition is true

# SYNOPSIS

```text
WHILE condition
    statements
ENDWHILE
```

# DESCRIPTION

Test before each iteration; the body may run zero times. ENDWHILE closes the loop. The standard WHILE header has no DO keyword; the compiler also accepts `WHILE condition DO` for compatibility.

# EXAMPLE

```text
DECLARE Count : INTEGER
Count <- 1
WHILE Count <= 3
    OUTPUT Count
    Count <- Count + 1
ENDWHILE
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-repeat(7)](cpsc-keyword-repeat.7.md), [cpsc-keyword-for(7)](cpsc-keyword-for.7.md).
