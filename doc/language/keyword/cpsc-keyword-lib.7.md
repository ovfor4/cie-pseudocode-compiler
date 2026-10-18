% CPSC-KEYWORD-LIB(7)

# NAME

cpsc-keyword-lib - mark a library compilation unit

# SYNOPSIS

```text
#LIB
```

# DESCRIPTION

A line-leading `#` (optionally preceded by whitespace) begins a compiler directive. Only `#LIB` is supported. It marks the entire source file as library code. Compile-time diagnostics retain library locations; runtime checks use the line of the first call from user code into library code, including methods, operators, automatic constructors/destructors and EXTERN calls. Library-to-library calls preserve that line. Directives are not comments.

C++ library objects may read `extern "C" int __cps_user_line`, print `[Fatal] line N: reason` to stderr and throw an anonymous integer exception for pseudocode TRY/CATCH. The global is shared by the program and is not thread-local.

# EXAMPLE

```text
#LIB
PROCEDURE Check()
    DECLARE A : ARRAY[1:1] OF INTEGER
    OUTPUT A[2]
ENDPROCEDURE
```

# SEE ALSO

[cpsc-keyword-include(7)](cpsc-keyword-include.7.md), [cpsc-keyword-try(7)](cpsc-keyword-try.7.md).
