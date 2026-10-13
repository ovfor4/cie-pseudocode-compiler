% CPSC-FUNCTION-CHR(7)

# NAME

cpsc-function-chr - convert a byte code to text

# SYNOPSIS

```text
CHR(code : INTEGER) RETURNS STRING
```

# DESCRIPTION

Convert the low eight bits of code to a one-byte string.
The result is STRING, not CHAR. Use ASCII codes 1 to 127 for
ordinary text; code 0 produces an empty string.

# EXAMPLE

```text
OUTPUT CHR(65)
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-function-asc(7)](cpsc-function-asc.7.md).

