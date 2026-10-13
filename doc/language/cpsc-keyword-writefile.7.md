% CPSC-KEYWORD-WRITEFILE(7)

# NAME

cpsc-keyword-writefile - write a line to a text file

# SYNOPSIS

```text
WRITEFILE filename, expression
```

# DESCRIPTION

Write one value as text followed by a newline. Open the file FOR WRITE or
APPEND first. Exactly one data expression is accepted; join strings with
`&` when needed. Records, classes, and pointers are unsupported as data.

# EXAMPLE

```text
OPENFILE "notes.txt" FOR APPEND
WRITEFILE "notes.txt", "Score: " & NUM_TO_STR(10)
CLOSEFILE "notes.txt"
```

# SEE ALSO

[cpsc-language-overview(7)](cpsc-language-overview.7.md), [cpsc-keyword-openfile(7)](cpsc-keyword-openfile.7.md), [cpsc-keyword-closefile(7)](cpsc-keyword-closefile.7.md), [cpsc-function-num_to_str(7)](cpsc-function-num_to_str.7.md).

