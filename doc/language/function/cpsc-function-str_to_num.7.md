% CPSC-FUNCTION-STR_TO_NUM(7)

# NAME

cpsc-function-str_to_num - convert text to a real number

# SYNOPSIS

```text
STR_TO_NUM(text : STRING) RETURNS REAL
STR_TO_NUM(text : CHAR) RETURNS REAL
```

# DESCRIPTION

Provided by the standard string library, loaded and linked by default.

Return REAL, including for integer-looking text. The whole string must be consumed. Invalid text and trailing junk throw a catchable exception reading `STR_TO_NUM: '<text>' is not a number`, and overflowing values throw `STR_TO_NUM: '<text>' is out of the REAL range`. Underflow is accepted. Use IS_NUM first to check numeric text without throwing for invalid input.

# EXAMPLE

```text
OUTPUT STR_TO_NUM("12.5")
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-function-is_num(7)](cpsc-function-is_num.7.md), [cpsc-function-num_to_str(7)](cpsc-function-num_to_str.7.md).
