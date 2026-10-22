% CPSC-KEYWORD-INPUT(7)

# NAME

cpsc-keyword-input - read a value from standard input

# SYNOPSIS

```text
INPUT target
```

# DESCRIPTION

Read into an INTEGER, REAL, BOOLEAN, CHAR, or STRING location: a previously declared variable, an individual array element, or a public record/class field, as allowed by guide section 5.1. Nested fields, fields of array elements and pointer dereferences are supported through the existing access syntax. INTEGER, REAL, BOOLEAN and CHAR input read one whitespace-delimited token; after it, spaces and tabs are skipped and one following newline is consumed. STRING input reads the rest of the current line, without its line ending, so `5` and `Ada` on one line, `5 Ada`, give 5 and "Ada", and `5` then `hello` on the next line gives 5 and "hello". INTEGER accepts an optionally signed decimal that fits INTEGER, REAL accepts what STR_TO_NUM accepts, BOOLEAN accepts TRUE or FALSE in any case, and CHAR needs exactly one character.

Input that does not fit the target type stops the program with a runtime error such as `[Fatal] line 3: Invalid INTEGER input 'abc'`; reading past the end of input stops it with `INPUT reached the end of input`. The error can be caught with TRY.

Native C scalar targets are also supported, using their actual widths and signedness, and the same token rules. Plain C char reads a one-character token; signed/unsigned char and the other integers read a number that must fit the type. C _Bool reads a number and stores whether it is nonzero. C pointers, including C_STRING, cannot be INPUT targets. See [cpsc-keyword-c_type(7)](cpsc-keyword-c_type.7.md).

The target address is evaluated once per INPUT execution, before reading the value. Array bounds, unset pointers and private fields are checked as for assignment. Constants, whole arrays, other whole records/classes, enums and pointer values cannot be INPUT targets. A dereferenced pointer may select a supported target type.

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
