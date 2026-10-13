% CPSC-KEYWORD-OUTPUT(7)

# NAME

cpsc-keyword-output - print values to standard output

# SYNOPSIS

```text
OUTPUT expression, ...
```

# DESCRIPTION

Print expressions from left to right without automatic separators, then add
a newline. Strings print their contents; BOOLEAN prints TRUE or FALSE.
Whole arrays and partially indexed arrays print their elements one per line;
a final array adds no extra newline. Records, classes, and pointers cannot
be printed directly.

# EXAMPLE

```text
DECLARE Score : INTEGER
Score <- 10
OUTPUT "Score: ", Score
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-input(7)](cpsc-keyword-input.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md).

