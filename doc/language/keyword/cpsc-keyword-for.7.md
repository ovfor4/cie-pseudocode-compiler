% CPSC-KEYWORD-FOR(7)

# NAME

cpsc-keyword-for, cpsc-keyword-to, cpsc-keyword-step, cpsc-keyword-next - iterate over an integer range

# SYNOPSIS

```text
FOR variable <- start TO finish [STEP increment]
    statements
NEXT [variable]
```

# DESCRIPTION

The loop variable must already be declared INTEGER. Both endpoints are inclusive. STEP defaults to 1; use a negative value to count down. STEP is evaluated once, while the finish expression is evaluated each iteration. The variable after NEXT is optional. If present, it must be on the same line as NEXT and name the loop variable. Use a nonzero STEP. TO, STEP, and NEXT belong to this syntax.

# EXAMPLE

```text
DECLARE Index : INTEGER
FOR Index <- 3 TO 1 STEP -1
    OUTPUT Index
NEXT Index
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md), [cpsc-keyword-while(7)](cpsc-keyword-while.7.md).
