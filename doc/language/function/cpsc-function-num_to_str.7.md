% CPSC-FUNCTION-NUM_TO_STR(7)

# NAME

cpsc-function-num_to_str - convert a number to text

# SYNOPSIS

```text
NUM_TO_STR(number : INTEGER or REAL) RETURNS STRING
```

# DESCRIPTION

Format INTEGER as signed decimal text and REAL with six digits after the decimal point.

# EXAMPLE

```text
OUTPUT NUM_TO_STR(42), " ", NUM_TO_STR(3.5)
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-function-str_to_num(7)](cpsc-function-str_to_num.7.md).
