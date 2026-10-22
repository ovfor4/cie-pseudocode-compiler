% CPSC-KEYWORD-OUTPUT(7)

# NAME

cpsc-keyword-output - print values to standard output

# SYNOPSIS

```text
OUTPUT expression, ...
```

# DESCRIPTION

Print expressions from left to right without automatic separators, then add a newline. Strings print their contents; BOOLEAN prints TRUE or FALSE. REAL values use the shortest decimal text that reads back as the same value (as Python's `repr`): fixed notation with at least one fractional digit when 1e-4 <= |x| < 1e16 (`2.0`, `87.5`, `0.0001`), otherwise scientific notation with an exponent of at least two digits (`1e-07`, `1.5e-07`, `1e+16`); special values print as `-0.0`, `inf`, `-inf` and `nan`. OUTPUT and NUM_TO_STR produce identical text. Whole arrays and partially indexed arrays print their elements one per line; a final array adds no extra newline. Other records, classes, and pointers cannot be printed directly.

C scalars print with their native type's format and C argument promotions. Plain char prints a character, other C integers print numbers, and _Bool prints TRUE or FALSE. C_STRING is a pointer: cast it to STRING to print its contents. See [cpsc-keyword-c_type(7)](cpsc-keyword-c_type.7.md).

# EXAMPLE

```text
DECLARE Score : INTEGER
Score <- 10
OUTPUT "Score: ", Score
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-input(7)](cpsc-keyword-input.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md).
