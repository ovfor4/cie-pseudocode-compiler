% CPSC-KEYWORD-READFILE(7)

# NAME

cpsc-keyword-readfile - read a line from a text file

# SYNOPSIS

```text
READFILE filename, variable
```

# DESCRIPTION

Read one line from a file opened FOR READ, removing the line ending. The target must be a plain, previously declared STRING variable. Guard reads with EOF; reading past the end, using an unopened file, or using the wrong mode throws an exception that TRY/CATCH can handle.

# EXAMPLE

```text
DECLARE Line : STRING
OPENFILE "notes.txt" FOR READ
WHILE NOT EOF("notes.txt")
    READFILE "notes.txt", Line
    OUTPUT Line
ENDWHILE
CLOSEFILE "notes.txt"
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-openfile(7)](cpsc-keyword-openfile.7.md), [cpsc-keyword-closefile(7)](cpsc-keyword-closefile.7.md), [cpsc-function-eof(7)](../function/cpsc-function-eof.7.md), [cpsc-keyword-try(7)](cpsc-keyword-try.7.md).
