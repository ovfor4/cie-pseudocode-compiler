% CPSC-FUNCTION-LEFT(7)

# NAME

cpsc-function-left - extract the beginning of a string

# SYNOPSIS

```text
LEFT(text : STRING, count : INTEGER) RETURNS STRING
```

# DESCRIPTION

Return up to count bytes from the beginning. Counts larger than the string length return the whole string; negative counts return an empty string.

# EXAMPLE

```text
OUTPUT LEFT("Hello", 2)
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-function-right(7)](cpsc-function-right.7.md), [cpsc-function-mid(7)](cpsc-function-mid.7.md).
