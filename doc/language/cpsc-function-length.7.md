% CPSC-FUNCTION-LENGTH(7)

# NAME

cpsc-function-length - find a string's length

# SYNOPSIS

```text
LENGTH(text : STRING) RETURNS INTEGER
```

# DESCRIPTION

Return the number of bytes before the string terminator. String operations
are byte-based, so this is not a Unicode character count.

# EXAMPLE

```text
OUTPUT LENGTH("Hello")
```

# SEE ALSO

[cpsc-language-overview(7)](cpsc-language-overview.7.md), [cpsc-function-mid(7)](cpsc-function-mid.7.md), [cpsc-function-left(7)](cpsc-function-left.7.md), [cpsc-function-right(7)](cpsc-function-right.7.md).

