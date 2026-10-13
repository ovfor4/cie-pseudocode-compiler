% CPSC-FUNCTION-STR_TO_NUM(7)

# NAME

cpsc-function-str_to_num - convert text to a real number

# SYNOPSIS

```text
STR_TO_NUM(text : STRING) RETURNS REAL
```

# DESCRIPTION

Return REAL, including for integer-looking text. Conversion can stop at
trailing text; invalid text returns zero. Use IS_NUM first when the whole
string must be a valid number.

# EXAMPLE

```text
OUTPUT STR_TO_NUM("12.5")
```

# SEE ALSO

[cpsc-language-overview(7)](cpsc-language-overview.7.md), [cpsc-function-is_num(7)](cpsc-function-is_num.7.md), [cpsc-function-num_to_str(7)](cpsc-function-num_to_str.7.md).

