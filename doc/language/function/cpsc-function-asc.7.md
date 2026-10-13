% CPSC-FUNCTION-ASC(7)

# NAME

cpsc-function-asc - find a character's byte code

# SYNOPSIS

```text
ASC(character : CHAR) RETURNS INTEGER
ASC(text : STRING) RETURNS INTEGER
```

# DESCRIPTION

Return the unsigned byte code of the character, or of the first byte of a string. Use a nonempty string; for ASCII characters this is the ASCII code.

# EXAMPLE

```text
OUTPUT ASC('A')
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-function-chr(7)](cpsc-function-chr.7.md).
