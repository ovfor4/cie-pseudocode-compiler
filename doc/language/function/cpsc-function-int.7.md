% CPSC-FUNCTION-INT(7)

# NAME

cpsc-function-int - take the integer part of a number

# SYNOPSIS

```text
INT(value : REAL) RETURNS INTEGER
INT(value : INTEGER) RETURNS INTEGER
```

# DESCRIPTION

Built into the compiler, so it also works with `-no-std`.

Return the integer part of a REAL, truncating toward zero: `INT(27.5415)` is 27 and `INT(-27.9)` is -27. An INTEGER argument is returned unchanged. A NaN or a value outside the INTEGER range stops the program with a runtime error.

REAL never converts to INTEGER implicitly, so assignments, FOR bounds, array bounds and indices, arguments and RETURN values of type REAL are compile errors; wrap them in INT.

# EXAMPLE

```text
DECLARE Whole : INTEGER
Whole <- INT(27.5415)
OUTPUT Whole
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-div(7)](../keyword/cpsc-keyword-div.7.md).
