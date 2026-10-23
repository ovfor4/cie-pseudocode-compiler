% CPSC-FUNCTION-IS_NUM(7)

# NAME

cpsc-function-is_num - test whether text represents a number

# SYNOPSIS

```text
IS_NUM(text : STRING) RETURNS BOOLEAN
```

# DESCRIPTION

Provided by the standard string library, loaded and linked by default.

Return true exactly when STR_TO_NUM would succeed: a nonempty string fully consumed by `strtod`, with a value inside the REAL range. After any leading whitespace, only the characters `0123456789.eE+-` may appear, so NaN, inf/infinity and hexadecimal forms are rejected. Leading whitespace, exponents (`1e3`), a leading `+`, `.5` and `5.` are accepted; trailing whitespace, trailing text, embedded NUL bytes and overflowing values such as `1e999` are rejected. Underflow to zero or a denormal is accepted.

# EXAMPLE

```text
OUTPUT IS_NUM("12.5")
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-function-str_to_num(7)](cpsc-function-str_to_num.7.md).
