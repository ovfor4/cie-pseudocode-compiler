% CPSC-FUNCTION-IS_NUM(7)

# NAME

cpsc-function-is_num - test whether text represents a number

# SYNOPSIS

```text
IS_NUM(text : STRING) RETURNS BOOLEAN
```

# DESCRIPTION

Return true for a nonempty string fully consumed by the runtime's
numeric conversion. Leading whitespace is accepted; trailing whitespace
or other trailing text is rejected.

# EXAMPLE

```text
OUTPUT IS_NUM("12.5")
```

# SEE ALSO

[cpsc-language-overview(7)](cpsc-language-overview.7.md), [cpsc-function-str_to_num(7)](cpsc-function-str_to_num.7.md).

