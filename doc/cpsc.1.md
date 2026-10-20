% CPSC(1)

# NAME

cpsc - compile and run CIE pseudocode

# SYNOPSIS

**cpsc** [**-r**] [**-S**] [**-o** *output*] *source* [*source* ...]

**cpsc** **-h**

# DESCRIPTION

Compile one or more pseudocode source files to a native executable, or to textual LLVM IR with **-S**. Options may appear among the source paths. Source input from stdin is not supported.

Without **-o**, save beside the entry source file, replacing its extension with `.exe` on Windows or `.out` elsewhere for an executable, or `.ll` for IR. With **-r** alone, run without saving a file.

Each input is a compilation unit. Use top-level `INCLUDE "lib.cps"` to import another input's exported definitions, including its transitive imports. Paths are relative to the including source; every included file must also appear on the command line. Repeated includes and include cycles are harmless. Giving the same input twice, including symbolic or hard links, is an error. Duplicate definitions across files are errors even if the files do not include each other; distinct parameter signatures may share a name when every definition specifies OVERLOAD.

Exactly one file may contain top-level executable statements (including top-level DECLARE). Alternatively, define a parameterless `FUNCTION MAIN RETURNS INTEGER` in one file and have no top-level executable statements anywhere. Builds containing only definitions and no MAIN fail with `error: no main program`. GLOBAL blocks declare shared primitive variables. INCLUDE, GLOBAL and MAIN entry points are compiler extensions beyond the Cambridge guide.

# OPTIONS

**-h**
:   Print only the documentation entry point: `man 1 cpsc` on Unix/macOS, or Start Menu > cpsc > cpsc Help on Windows. Return successfully without compiling, even when other arguments are present.

**-o** *output*
:   Save to the specified path. Without **-S**, save a native executable; with **-S**, save LLVM IR. Use the path exactly as given; its extension does not select the output format. May be specified only once. With **-r**, keep this file even if execution fails.

**-S**
:   Save textual LLVM IR instead of a native executable. Without **-o**, use the entry source filename with a `.ll` extension beside the source file. May be combined with **-r** to save IR and run the program.

**-r**
:   Run the compiled program with the runner selected at build time (see EXECUTION MODES). Save output first when **-S** or **-o** is also present; otherwise do not save a file. The program inherits stdin, stdout and stderr; `INPUT` can read terminal or piped input.

# NATIVE COMPILATION

Native compilation uses LLVM, the Clang driver and LLD embedded in `cpsc`. Required startup objects, runtime archives and system-library link inputs are embedded too and unpacked temporarily when linking. No external compiler, linker or development SDK is needed. Generate programs for the compiler's own operating system and CPU architecture; cross-compilation is not supported.

On Linux, both `cpsc` and generated executables statically link the C++ runtime and retain dynamic dependencies on glibc and its system libraries. They require a compatible system glibc; static linking does not remove that requirement. Windows links non-system runtimes statically and uses Windows system DLLs. macOS uses operating-system libraries, including the system C++ runtime.

# EXECUTION MODES

The executable has one of the following execution modes, selected when it was built. All modes can generate native executables and LLVM IR; the mode determines whether and how **-r** runs the program. Running uses the selected runner even when a native executable is also saved.

**PATH**
:   Run `lli` from LLVM 23 in PATH. With **-r**, check the selected executable's version before compilation or output writing; a missing executable, a different major version or a failed or unrecognized version check is a fatal error. Version detection has a five-second timeout. Without **-r**, no external tools are checked. Execution uses temporary `.ll` IR unless **-S** saves IR; temporary IR is deleted afterward, including on execution failure.

**EMBEDDED**
:   Run the statically linked LLVM engine inside `cpsc`, without looking for `lli` in PATH. Execute the module in memory. Users do not need LLVM or `lli` installed; the executable may still depend on the operating system's runtime libraries.

**NONE**
:   Reject **-r** with an error before compilation or output writing. Native compilation and IR output remain available.

# EXAMPLES

Compile and save `test.out` (`test.exe` on Windows) beside `test.cps`:

```sh
cpsc test.cps
```

Choose the executable's output path:

```sh
cpsc test.cps -o result
```

Save LLVM IR as `test.ll` beside `test.cps`:

```sh
cpsc test.cps -S
```

Run without saving output:

```sh
cpsc test.cps -r
```

Save IR and run it:

```sh
cpsc test.cps -r -S -o result.ll
```

Save a native program and run it on Unix:

```sh
cpsc test.cps -o test.out
./test.out
```

Compile a program whose `me.cps` contains `INCLUDE "lib.cps"`:

```sh
cpsc me.cps lib.cps -r
```

# EXIT STATUS

**0**
:   Compilation succeeded, or **-h** printed the documentation entry point.

**1**
:   An argument, file, compilation or execution-engine error occurred, including missing input, when PATH mode cannot validate `lli` from LLVM 23 or NONE mode receives **-r**.

With **-r**, return the executed program's exit status. MAIN's INTEGER result is truncated to 32 bits before returning to the operating system. In PATH mode, failure to launch `lli` or a child-process crash returns 1.

An uncaught program exception returns 1 after printing `Unhandled exception. Program terminated.` to standard error. Runtime check failures print the source line and error reason to standard error before throwing and can be handled with TRY/CATCH/ENDTRY.

# NOTES

Compiler diagnostics go to stderr. With multiple input files, lexer, parser and code generation diagnostics include the source path after the line number. Failed source compilation does not write output; failed native linking does not replace an existing output file. Successful compilation overwrites an existing output file, but an output path that refers to any input source file (including a symbolic or hard link) is rejected. Runtime diagnostics of the compiled program (for example `[Fatal] line N: ...` and `Unhandled exception. Program terminated.`) are also written to stderr, so `OUTPUT` text alone appears on stdout.

Missing input, including invoking `cpsc` without arguments, reports `cpsc: fatal error: no input files`. Ordinary lexical, syntax and semantic errors use `error:`; failures that prevent compilation, such as file I/O errors, use `fatal error:`. Compilation and argument failures end with one `compilation terminated.` line. Execution-stage failures do not print that line.

Generated IR uses the C++ exception ABI. Native binaries need the C++ runtime; EMBEDDED includes it, while PATH uses the runtime available to `lli`. Nightly CI runs embedded exception tests on Linux, Windows and macOS with LLVM 23.

# SEE ALSO

[cpsc-language-overview(7)](language/cpsc-language-overview.7.md) describes supported syntax and links to the keyword and built-in function manuals.
