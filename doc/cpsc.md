% CPSC(1)

# NAME

cpsc - compile and run CIE pseudocode

# SYNOPSIS

**cpsc** [**-r**] [**-o** *output*] *source*

# DESCRIPTION

Compile one pseudocode source file to LLVM IR. Options may appear before or after the source path. Source input from stdin is not supported.

Without **-o** or **-r**, replace the source filename's extension with `.ir` and save the output beside the source file.

# OPTIONS

**-o** *output*
:   Save IR to the specified path. May be specified only once. With **-r**, keep this file even if execution fails.

**-r**
:   Run the compiled program using `lli` from PATH. Without **-o**, use a temporary IR file and delete it afterward, including on execution failure. The program inherits stdin, stdout and stderr; `INPUT` can read terminal or piped input.

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

# EXIT STATUS

**0**
:   Compilation succeeded. Invoking `cpsc` with no arguments also exits silently with status 0.

**1**
:   An argument, file, compilation or execution-launch error occurred, including when `lli` cannot be found in PATH.

With **-r**, return `lli`'s exit status. Failure to launch it or a process crash returns 1.

# NOTES

Compiler diagnostics go to stderr. Failed compilation does not write IR. Successful compilation overwrites an existing output file, but an output path that refers to the source file (including a symbolic or hard link) is rejected.
