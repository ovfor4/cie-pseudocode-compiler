% CPSC-KEYWORD-CLASS(7)

# NAME

cpsc-keyword-class - define a class with public and private fields

# SYNOPSIS

```text
CLASS name
    PUBLIC field : type
    PRIVATE field : type
ENDCLASS
```

# DESCRIPTION

Declare a class at top level with at least one field. Every field explicitly
requires PUBLIC or PRIVATE, without DECLARE. This differs from guide
section 10's default-public rule. ENDCLASS closes the declaration.

Classes use record value semantics: zero initialization, whole-value copying,
arrays, pointers, and ordinary function/procedure parameters. Only PUBLIC
fields can be accessed externally. PRIVATE fields still initialize and copy
with the whole value. Methods, constructors, destructors, NEW, INHERITS,
and array fields are unsupported.

# EXAMPLE

```text
CLASS Person
    PUBLIC Name : STRING
    PRIVATE Age : INTEGER
ENDCLASS
DECLARE PersonValue : Person
PersonValue.Name <- "Alice"
OUTPUT PersonValue.Name
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-type(7)](cpsc-keyword-type.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md).

