% CPSC-FUNCTION-NUM_TO_STR(7)

# NAME

cpsc-function-num_to_str - convert a number to text

# SYNOPSIS

```text
NUM_TO_STR(number : INTEGER or REAL) RETURNS STRING
```

# DESCRIPTION

Provided by the standard string library, loaded and linked by default.

Format INTEGER as signed decimal text and REAL in the same shortest round-trip form that OUTPUT prints. The text is the shortest decimal text that reads back as the same value (as Python's `repr`): fixed notation with at least one fractional digit when 1e-4 <= |x| < 1e16 (`2.0`, `87.5`, `0.0001`), otherwise scientific notation with an exponent of at least two digits (`1e-07`, `1.5e-07`, `1e+16`); special values print as `-0.0`, `inf`, `-inf` and `nan`.

# EXAMPLE

```text
OUTPUT NUM_TO_STR(42), " ", NUM_TO_STR(3.5)
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-function-str_to_num(7)](cpsc-function-str_to_num.7.md).
