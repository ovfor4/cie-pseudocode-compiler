% CPSC-KEYWORD-DECLARE(7)

# NAME

cpsc-keyword-declare - declare variables and arrays

# SYNOPSIS

```text
DECLARE name : type
DECLARE name : ARRAY[lower:upper, ...] OF type
```

# DESCRIPTION

Declare one variable per statement. Built-in types are INTEGER (signed 64-bit),
REAL, BOOLEAN, CHAR (one byte), and STRING. User-defined TYPE and CLASS names
are also accepted. Variables start with zero, FALSE, an empty string, or a
zero-initialized user-defined value.

ARRAY bounds are inclusive and may be expressions; multiple ranges create
multiple dimensions. Access elements with `name[index, ...]`. Bounds are
checked at run time. Whole-array assignment is unsupported. Declare arrays
before using them; arrays declared inside a control-flow block stay in that
block.

# EXAMPLE

```text
DECLARE Count : INTEGER
DECLARE Names : ARRAY[1:3] OF STRING
Count <- 2
Names[Count] <- "Alice"
OUTPUT Names[Count]
```

# SEE ALSO

[cpsc-language-overview(7)](cpsc-language-overview.7.md), [cpsc-keyword-type(7)](cpsc-keyword-type.7.md), [cpsc-keyword-class(7)](cpsc-keyword-class.7.md), [cpsc-keyword-output(7)](cpsc-keyword-output.7.md).

