% CPSC-KEYWORD-CASE(7)

# NAME

cpsc-keyword-case - match values or ranges in source order

# SYNOPSIS

```text
CASE OF identifier
    value : statements
    lower TO upper : statements
    OTHERWISE : statements
ENDCASE
```

# DESCRIPTION

Read the selector once, then test labels in source order. Execute only the first matching branch. If nothing matches, execute the optional final OTHERWISE branch, or continue after ENDCASE. There is no fall-through.

Each label header must start on a new line and end with a colon on that line. Its body may start after the colon or on following lines and may contain multiple statements or nested blocks. Indentation is for readability.

Labels and range endpoints may be expressions, including function calls. A single value uses ordinary equality; a TO range includes both endpoints and uses ordinary ordering. Evaluate the lower endpoint first and evaluate the upper endpoint only if the lower comparison succeeds. Do not evaluate later labels after a match. Unlike CONSTANT initializers, CASE labels are not restricted to literals.

STRING comparisons use contents. Enumerations and explicit pointer types follow their ordinary comparison rules; pointers support equality only. Records and whole arrays cannot be compared. The compiler emits conditional branches and disables jump tables for functions containing CASE.

# EXAMPLE

```text
DECLARE Move : CHAR
INPUT Move
CASE OF Move
    'W' : OUTPUT "up"
    'A' :
        OUTPUT "left"
        OUTPUT "turn"
    OTHERWISE : OUTPUT "other"
ENDCASE
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-if(7)](cpsc-keyword-if.7.md).
