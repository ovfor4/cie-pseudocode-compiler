% CPSC(1)

# NAME

cpsc - compile and run CIE pseudocode

# SYNOPSIS

**cpsc** [**-r**] [**-o** *output*] *source*

**cpsc** **-h**

# DESCRIPTION

Compile one pseudocode source file to LLVM IR. Options may appear before or after the source path. Source input from stdin is not supported.

Without **-o** or **-r**, replace the source filename's extension with `.ir` and save the output beside the source file.

# OPTIONS

**-h**
:   Print only the documentation entry point: `man 1 cpsc` on Unix/macOS, or Start Menu > cpsc > cpsc Help on Windows. Return successfully without compiling, even when other arguments are present.

**-o** *output*
:   Save IR to the specified path. May be specified only once. With **-r**, keep this file even if execution fails.

**-r**
:   Run the compiled program with the runner selected at build time (see EXECUTION MODES). Without **-o**, do not save permanent IR. The program inherits stdin, stdout and stderr; `INPUT` can read terminal or piped input.

# EXECUTION MODES

The executable has one of the following execution modes, selected when it was built. All modes can compile to LLVM IR; the mode determines whether and how **-r** runs the program.

**PATH**
:   Run `lli` from PATH. If it cannot be found, **-r** fails before compilation or output writing. Without **-o**, write temporary IR and delete it afterward, including on execution failure.

**EMBEDDED**
:   Run the statically linked LLVM engine inside `cpsc`, without looking for `lli` in PATH. Without **-o**, execute the module in memory. Users do not need LLVM or `lli` installed; the executable may still depend on the operating system's C runtime libraries.

**NONE**
:   Reject **-r** with an error before compilation or output writing. Compilation to IR remains available.

# EXAMPLES

Compile and save `test.ir` beside `test.cps`:

```sh
cpsc test.cps
```

Choose the output path:

```sh
cpsc test.cps -o result.ir
```

Run without saving permanent IR:

```sh
cpsc test.cps -r
```

Save IR and run it:

```sh
cpsc test.cps -r -o result.ir
```

# CLASSES

Declare classes at top level. Every field must explicitly specify `PUBLIC` or
`PRIVATE`, without `DECLARE`:

```text
CLASS Person
    PUBLIC Name : STRING
    PRIVATE Age : INTEGER
ENDCLASS

DECLARE p : Person
p.Name <- "Alice"
OUTPUT p.Name
```

Classes use record value semantics: zero initialization, whole-value copying,
arrays, pointers and ordinary function/procedure parameters. Private fields cannot
be read, written or addressed externally; they are still initialized and copied
with the whole value. Record `TYPE` fields remain implicitly public and cannot
specify access levels. Class methods, constructors, destructors, `NEW` and
`INHERITS` are not supported. Array fields remain unsupported in both records and
classes.

# EXIT STATUS

**0**
:   Compilation succeeded, or **-h** printed the documentation entry point.

**1**
:   An argument, file, compilation or execution-engine error occurred, including missing input, when PATH mode cannot find `lli` or NONE mode receives **-r**.

With **-r**, return the executed program's exit status. In PATH mode, failure to launch `lli` or a child-process crash returns 1.

# NOTES

Compiler diagnostics go to stderr. Failed compilation does not write IR. Successful compilation overwrites an existing output file, but an output path that refers to the source file (including a symbolic or hard link) is rejected.

Missing input, including invoking `cpsc` without arguments, reports `cpsc: fatal error: no input files`. Ordinary lexical, syntax and semantic errors use `error:`; failures that prevent compilation, such as file I/O errors, use `fatal error:`. Compilation and argument failures end with one `compilation terminated.` line. Execution-stage failures do not print that line.
