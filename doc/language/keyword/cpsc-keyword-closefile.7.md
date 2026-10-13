% CPSC-KEYWORD-CLOSEFILE(7)

# NAME

cpsc-keyword-closefile - close a text file

# SYNOPSIS

```text
CLOSEFILE filename
```

# DESCRIPTION

Close the file associated with this filename string and release its open-file
slot. Closing a filename that is not open terminates execution.

# EXAMPLE

```text
OPENFILE "notes.txt" FOR WRITE
WRITEFILE "notes.txt", "Done"
CLOSEFILE "notes.txt"
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-openfile(7)](cpsc-keyword-openfile.7.md), [cpsc-keyword-readfile(7)](cpsc-keyword-readfile.7.md), [cpsc-keyword-writefile(7)](cpsc-keyword-writefile.7.md).

