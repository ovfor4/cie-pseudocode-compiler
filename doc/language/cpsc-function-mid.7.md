% CPSC-FUNCTION-MID(7)

# NAME

cpsc-function-mid - extract part of a string

# SYNOPSIS

```text
MID(text : STRING, start : INTEGER, count : INTEGER) RETURNS STRING
```

# DESCRIPTION

Return up to count bytes starting at the one-based position start.
Positions below 1 clamp to 1; positions beyond the end return an empty string.
The count clamps to the available bytes; negative counts return an empty string.

# EXAMPLE

```text
OUTPUT MID("ABCDEFGH", 2, 3)
```

# SEE ALSO

[cpsc-language-overview(7)](cpsc-language-overview.7.md), [cpsc-function-left(7)](cpsc-function-left.7.md), [cpsc-function-right(7)](cpsc-function-right.7.md), [cpsc-function-length(7)](cpsc-function-length.7.md).

