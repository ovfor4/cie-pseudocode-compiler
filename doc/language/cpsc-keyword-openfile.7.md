% CPSC-KEYWORD-OPENFILE(7)

# NAME

cpsc-keyword-openfile - open a text file

# SYNOPSIS

```text
OPENFILE filename FOR READ
OPENFILE filename FOR WRITE
OPENFILE filename FOR APPEND
```

# DESCRIPTION

The filename is a STRING expression. READ opens an existing file for reading;
WRITE creates or truncates a file; APPEND writes at the end, creating the file
if needed. READ, WRITE, APPEND, and this use of FOR belong here.

At most 16 files may be open at once. Use the same filename string for
subsequent operations. Reopening an open filename, failed opens, and wrong-mode
operations terminate execution with a fatal diagnostic. RANDOM is reserved
but unsupported.

# EXAMPLE

```text
OPENFILE "notes.txt" FOR WRITE
WRITEFILE "notes.txt", "Hello"
CLOSEFILE "notes.txt"
```

# SEE ALSO

[cpsc-language-overview(7)](cpsc-language-overview.7.md), [cpsc-keyword-readfile(7)](cpsc-keyword-readfile.7.md), [cpsc-keyword-writefile(7)](cpsc-keyword-writefile.7.md), [cpsc-keyword-closefile(7)](cpsc-keyword-closefile.7.md).

