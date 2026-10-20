% CPSC-FUNCTION-LCASE(7)

# NAME

cpsc-function-lcase - convert text to lower case

# SYNOPSIS

```text
LCASE(character : CHAR) RETURNS CHAR
LCASE(text : STRING) RETURNS STRING
```

# DESCRIPTION

Provided by the standard string library, loaded and linked by default.

Return a copy with letters converted to lower case. The CHAR overload follows guide section 5.5; the STRING overload is a compatibility extension. Use ASCII text for predictable character conversion.

# EXAMPLE

```text
OUTPUT LCASE("HELLO")
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-function-ucase(7)](cpsc-function-ucase.7.md).
