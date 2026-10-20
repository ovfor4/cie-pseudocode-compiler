% CPSC-KEYWORD-GLOBAL(7)

# NAME

cpsc-keyword-global, cpsc-keyword-endglobal - declare shared primitive variables

# SYNOPSIS

```text
GLOBAL
    DECLARE name : type
    name <- literal
ENDGLOBAL
```

# DESCRIPTION

GLOBAL and ENDGLOBAL are compiler extensions. Any number of GLOBAL blocks may appear at top level. Their bodies allow only DECLARE of INTEGER, REAL, CHAR, STRING or BOOLEAN variables and literal assignments to declared global variables. Negative numeric literals are allowed. Arrays, records, other classes, C aliases, constructor arguments, calls, NEW and computed expressions are unsupported.

Global variables have program lifetime and shared writable storage. They are visible throughout their own source file and in files importing them with INCLUDE, including inside functions and methods. Local variables and parameters may shadow them. Literal initializers are established before user code runs. Omitted initializers use zero or FALSE; STRING globals construct empty owning values, and STRING literal assignments copy their contents.

A GLOBAL block does not count as top-level executable code. A build still requires top-level executable statements in exactly one file, or a parameterless FUNCTION MAIN returning INTEGER.

# EXAMPLE

```text
GLOBAL
    DECLARE Counter : INTEGER
    Counter <- 2
ENDGLOBAL

PROCEDURE Increment
    Counter <- Counter + 1
ENDPROCEDURE

FUNCTION MAIN RETURNS INTEGER
    CALL Increment()
    OUTPUT Counter
    RETURN 0
ENDFUNCTION
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-include(7)](cpsc-keyword-include.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md).
