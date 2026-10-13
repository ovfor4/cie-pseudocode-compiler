% CPSC-FUNCTION-EOF(7)

# NAME

cpsc-function-eof - test for the end of a text file

# SYNOPSIS

```text
EOF(filename : STRING) RETURNS BOOLEAN
```

# DESCRIPTION

Return true when no further characters remain in a file opened FOR READ.
The test does not consume the next character. Testing an unopened file or
a file in the wrong mode terminates execution.

# EXAMPLE

```text
DECLARE Line : STRING
OPENFILE "notes.txt" FOR READ
WHILE NOT EOF("notes.txt") DO
    READFILE "notes.txt", Line
    OUTPUT Line
ENDWHILE
CLOSEFILE "notes.txt"
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-openfile(7)](../keyword/cpsc-keyword-openfile.7.md), [cpsc-keyword-readfile(7)](../keyword/cpsc-keyword-readfile.7.md), [cpsc-keyword-closefile(7)](../keyword/cpsc-keyword-closefile.7.md).

