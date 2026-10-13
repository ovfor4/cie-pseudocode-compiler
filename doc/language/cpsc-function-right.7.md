% CPSC-FUNCTION-RIGHT(7)

# NAME

cpsc-function-right - extract the end of a string

# SYNOPSIS

```text
RIGHT(text : STRING, count : INTEGER) RETURNS STRING
```

# DESCRIPTION

Return up to count bytes from the end. Use a nonnegative count.
Counts larger than the string length return the whole string.

# EXAMPLE

```text
OUTPUT RIGHT("Hello", 2)
```

# SEE ALSO

[cpsc-language-overview(7)](cpsc-language-overview.7.md), [cpsc-function-left(7)](cpsc-function-left.7.md), [cpsc-function-mid(7)](cpsc-function-mid.7.md).

