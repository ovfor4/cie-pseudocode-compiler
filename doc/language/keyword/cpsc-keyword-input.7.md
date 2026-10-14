% CPSC-KEYWORD-INPUT(7)

# NAME

cpsc-keyword-input - read a value from standard input

# SYNOPSIS

```text
INPUT variable
```

# DESCRIPTION

Read into a previously declared INTEGER, REAL, BOOLEAN, CHAR, or STRING variable. STRING input reads one whitespace-delimited word. The target must be a plain variable; array elements and record/class fields are unsupported, unlike the wider INPUT syntax in guide section 5.1.

Native C scalar variables are also supported, using their actual widths and signedness. Plain C char reads a character; signed/unsigned char read numbers. C _Bool reads a number and stores whether it is nonzero. C pointers, including C_STRING, cannot be INPUT targets. See [cpsc-keyword-c_type(7)](cpsc-keyword-c_type.7.md).

# EXAMPLE

```text
DECLARE Name : STRING
INPUT Name
OUTPUT Name
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md), [cpsc-keyword-output(7)](cpsc-keyword-output.7.md).
