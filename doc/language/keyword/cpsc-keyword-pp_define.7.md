% CPSC-KEYWORD-PP_DEFINE(7)

# NAME

cpsc-keyword-pp_define, cpsc-keyword-pp_undef - define and remove preprocessor switches

# SYNOPSIS

```text
PP_DEFINE name
PP_UNDEF name
```

# DESCRIPTION

PP_DEFINE and PP_UNDEF are compiler extensions processed before ordinary source code is tokenized. Each directive occupies its own line, optionally preceded by whitespace and followed by a // comment. Names contain ASCII letters, digits and underscores, starting with a letter or underscore. Directive keywords are uppercase; switch names are case-sensitive, unlike ordinary pseudocode identifiers.

PP_DEFINE enables a switch and PP_UNDEF removes it. Defining an already defined switch or removing an undefined switch has no effect. Switches have no values, do not replace source text and use a namespace separate from ordinary identifiers. Extra arguments are errors. There are no command-line switch options.

Each source file starts with no switches defined. Definitions take effect in source order and continue until removed or the file ends, including across function boundaries. INCLUDE imports symbols from independently preprocessed files; switches never pass between files. Definitions in a disabled conditional region have no effect. See [cpsc-keyword-pp_ifdef(7)](cpsc-keyword-pp_ifdef.7.md) for conditional compilation.

The special switch __LIB marks subsequent source positions as library code. PP_UNDEF __LIB restores ordinary code behavior for subsequent positions; both directives may appear inside a function. Runtime checks in ordinary code report their own source line. Checks in library code use the shared user call line: calls from ordinary code update it, while calls from library code preserve it. This also covers methods, operators, EXTERN calls and automatic construction or destruction. Templates retain the modes at their original definition positions. Compile-time diagnostics always retain their original source file and line. The former #LIB syntax is rejected.

C++ library objects may read `extern "C" int __cps_user_line`, print `[Fatal] line N: reason` to stderr and throw an anonymous integer exception for pseudocode TRY/CATCH. The global is shared by the program and is not thread-local.

# EXAMPLE

```text
PP_DEFINE __LIB
PROCEDURE Check()
    DECLARE A : ARRAY[1:1] OF INTEGER
    OUTPUT A[2]
ENDPROCEDURE
PP_UNDEF __LIB

TRY
    CALL Check() // The library bounds check reports this call's line.
CATCH
    OUTPUT "Bounds error caught"
ENDTRY
```

# SEE ALSO

[cpsc-keyword-pp_ifdef(7)](cpsc-keyword-pp_ifdef.7.md), [cpsc-keyword-include(7)](cpsc-keyword-include.7.md), [cpsc-keyword-try(7)](cpsc-keyword-try.7.md).
