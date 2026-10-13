% CPSC-FUNCTION-LCASE(7)

# NAME

cpsc-function-lcase - convert text to lower case

# SYNOPSIS

```text
LCASE(text : STRING) RETURNS STRING
```

# DESCRIPTION

Return a copy with letters converted to lower case. Unlike guide section 5.5's
CHAR-to-CHAR signature, cpsc accepts text and returns STRING. Use ASCII
text for predictable character conversion.

# EXAMPLE

```text
OUTPUT LCASE("HELLO")
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-function-ucase(7)](cpsc-function-ucase.7.md).

