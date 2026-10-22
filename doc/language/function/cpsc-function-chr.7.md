% CPSC-FUNCTION-CHR(7)

# NAME

cpsc-function-chr - convert a byte code to text

# SYNOPSIS

```text
CHR(code : INTEGER) RETURNS STRING
```

# DESCRIPTION

Provided by the standard string library, loaded and linked by default.

Convert the low eight bits of code to a one-byte string. The result is STRING, not CHAR, but a STRING of length 1 converts implicitly to CHAR on assignment. Use ASCII codes 1 to 127 for ordinary text; code 0 produces one NUL byte, so LENGTH(CHR(0)) is 1.

# EXAMPLE

```text
OUTPUT CHR(65)
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-function-asc(7)](cpsc-function-asc.7.md).
