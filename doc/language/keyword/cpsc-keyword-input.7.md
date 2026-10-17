% CPSC-KEYWORD-INPUT(7)

# NAME

cpsc-keyword-input - read a value from standard input

# SYNOPSIS

```text
INPUT target
```

# DESCRIPTION

Read into an INTEGER, REAL, BOOLEAN, CHAR, or STRING location: a previously declared variable, an individual array element, or a public record/class field, as allowed by guide section 5.1. Nested fields, fields of array elements and pointer dereferences are supported through the existing access syntax. STRING input reads one whitespace-delimited word.

Native C scalar targets are also supported, using their actual widths and signedness. Plain C char reads a character; signed/unsigned char read numbers. C _Bool reads a number and stores whether it is nonzero. C pointers, including C_STRING, cannot be INPUT targets. See [cpsc-keyword-c_type(7)](cpsc-keyword-c_type.7.md).

The target address is evaluated once per INPUT execution, before reading the value. Array bounds, unset pointers and private fields are checked as for assignment. Constants, whole arrays, whole records/classes, enums and pointer values cannot be INPUT targets. A dereferenced pointer may select a supported target type.

# EXAMPLE

```text
DECLARE Scores : ARRAY[1:3] OF INTEGER
DECLARE j : INTEGER
FOR j <- 1 TO 3
    INPUT Scores[j]
NEXT j
OUTPUT Scores

TYPE Student
    DECLARE Score : INTEGER
ENDTYPE
CLASS Box
    Value : INTEGER
ENDCLASS
DECLARE Person : Student
DECLARE Item : Box
INPUT Person.Score
INPUT Item.Value
OUTPUT Person.Score, ":", Item.Value
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md), [cpsc-keyword-output(7)](cpsc-keyword-output.7.md).
