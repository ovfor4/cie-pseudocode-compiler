% CPSC-FUNCTION-UCASE(7)

# NAME

cpsc-function-ucase - convert text to upper case

# SYNOPSIS

```text
UCASE(text : STRING) RETURNS STRING
```

# DESCRIPTION

Return a copy with letters converted to upper case. Unlike guide section 5.5's
CHAR-to-CHAR signature, cpsc accepts text and returns STRING. Use ASCII
text for predictable character conversion.

# EXAMPLE

```text
OUTPUT UCASE("Hello")
```

# SEE ALSO

[cpsc-language-overview(7)](cpsc-language-overview.7.md), [cpsc-function-lcase(7)](cpsc-function-lcase.7.md).

