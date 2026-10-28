% CPSC-FUNCTION-STR_TO_NUM(7)

# NAME

cpsc-function-str_to_num - convert text to a number

# SYNOPSIS

```text
STR_TO_NUM(text : STRING) RETURNS DEFAULT REAL
STR_TO_NUM(text : CHAR) RETURNS DEFAULT REAL
STR_TO_NUM(text : STRING) RETURNS INTEGER
STR_TO_NUM(text : CHAR) RETURNS INTEGER
```

# DESCRIPTION

Provided by the standard string library, loaded and linked by default.

The function table in the 9618 winter 2025 insert allows STRING or CHAR input and REAL or INTEGER output. The compiler selects the return type from context and defaults to REAL, including for integer-looking text. The INTEGER version parses as REAL and applies INT, truncating toward zero with the existing INTEGER range check; it inherits REAL rounding. For example, assigning STR_TO_NUM("-23.45") to an INTEGER gives -23. DEFAULT selection and this truncation policy are compiler extensions.

The whole string must be consumed. Invalid text and trailing junk throw a catchable exception reading `STR_TO_NUM: '<text>' is not a number`, and REAL overflow throws `STR_TO_NUM: '<text>' is out of the REAL range`. Underflow is accepted. INTEGER overflow uses INT's range error. Use IS_NUM first to check numeric text without throwing for invalid input; it does not check INTEGER range.

# EXAMPLE

```text
OUTPUT STR_TO_NUM("12.5")
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-default(7)](../keyword/cpsc-keyword-default.7.md), [cpsc-function-int(7)](cpsc-function-int.7.md), [cpsc-function-is_num(7)](cpsc-function-is_num.7.md), [cpsc-function-num_to_str(7)](cpsc-function-num_to_str.7.md).
