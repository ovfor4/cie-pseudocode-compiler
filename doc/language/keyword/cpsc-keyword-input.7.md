% CPSC-KEYWORD-INPUT(7)

# NAME

cpsc-keyword-input - read a value from standard input

# SYNOPSIS

```text
INPUT variable
```

# DESCRIPTION

Read into a previously declared INTEGER, REAL, BOOLEAN, CHAR, or STRING variable. STRING input reads one whitespace-delimited word. The target must be a plain variable; array elements and record/class fields are unsupported, unlike the wider INPUT syntax in guide section 5.1.

# EXAMPLE

```text
DECLARE Name : STRING
INPUT Name
OUTPUT Name
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md), [cpsc-keyword-output(7)](cpsc-keyword-output.7.md).
