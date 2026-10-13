% CPSC-KEYWORD-IF(7)

# NAME

cpsc-keyword-if - choose between branches

# SYNOPSIS

```text
IF condition THEN
    statements
ELSE
    statements
ENDIF
```

# DESCRIPTION

Execute the THEN branch when the condition is true, otherwise the optional
ELSE branch. Blocks may be nested. THEN, ELSE, and ENDIF belong to this syntax.

# EXAMPLE

```text
IF 3 > 2 THEN
    OUTPUT "yes"
ELSE
    OUTPUT "no"
ENDIF
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-and(7)](cpsc-keyword-and.7.md), [cpsc-keyword-or(7)](cpsc-keyword-or.7.md), [cpsc-keyword-not(7)](cpsc-keyword-not.7.md).

