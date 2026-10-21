# Driver behavior

Driver behavior (`tools/driver/main.cc`):

- One or more source files, with optional `-S`, `-o path`, `-r` and `-no-std`; options may appear among inputs.
  Each stream is parsed independently. INCLUDE paths are relative to the including file,
  matched against command-line inputs with fs::equivalent. The driver rejects repeated
  inputs and computes transitive symbol visibility; cycles do not copy statements.
  CodeGen records owners for signatures, types and exported constants/globals and filters
  lookup by the current unit. Cross-file duplicate definitions are errors; distinct explicit OVERLOAD signatures may share a name.
  The entry file contains the only top-level code, or parameterless FUNCTION MAIN
  returning INTEGER; MAIN plus top-level code and builds without either entry are errors.
  SourceUnit borrows parsed statements for compilation. Type names are reserved across
  all units before payloads are completed, with included units visited first.
  GLOBAL declarations and literal initializers are emitted before function bodies.
  Own constants retain source-order visibility; imported constants are visible from the start.
  No stdin source input. Missing input, including no arguments, prints
  `cpsc: fatal error: no input files` and `compilation terminated.`, then returns 1.
- `-h` anywhere in the arguments prints one documentation pointer to stdout and
  returns 0 without compiling: `man 1 cpsc` on Unix/macOS, or
  Start Menu > cpsc > cpsc Help on Windows (selected by `_WIN32`).
- Output is written only after successful parsing, code generation and module
  verification. Diagnostics stay on stderr; source errors preserve existing output.
  Native linking uses a staged file beside the output and renames it on success.
- `-S` selects textual IR; otherwise output is a native executable. Without `-o`,
  replace the entry source extension with `.ll`, or with `.exe` on Windows / `.out` elsewhere
  for an executable, beside the source.
  `-o` paths are used literally and never select the format. `-r` alone saves nothing;
  either `-S` or `-o` requests saving before execution.
- Lexer, Parser and CodeGen track error flags (`hadError()`); any error, including
  invalid IR from `verifyModule`, makes the driver return 1. File/argument errors
  also return 1. Ordinary compiler errors use `error:`; failures that prevent
  compilation, such as file I/O errors, use `fatal error:`. The driver prints
  `compilation terminated.` once on compilation failure, including argument errors.
  Multi-input lexer/parser/codegen diagnostics append the source path after the line number;
  Diagnostics.h keeps single-input diagnostic text unchanged.
  Execution-stage failures do not print that line. Check the exit code rather
  than grepping stderr.
- Successful compilation overwrites the output, but paths aliasing any input source
  (including symlinks/hard links) are rejected.
- In PATH mode, `-r` executes PATH's `lli` using LLVM Support process APIs, with
  inherited stdin/stdout/stderr and no shell. Missing lli is rejected before
  compiling or writing output. The selected `lli --version` must identify LLVM 23
  and succeed within five seconds; other or unrecognized versions fail before
  source compilation. Without `-r`, no external tools are checked; clang/clang++
  are never invoked or checked by this mode. Process-launch/crash failures return 1.
  `-r` alone uses temporary IR removed on return, including execution failure.
- In NONE mode, `-r` returns 1 before compiling or writing output; native and IR
  compilation remain available. Only EMBEDDED uses the in-process execution engine.
- Both execution modes return the program's exit status and inherit standard
  streams. `-r -o` saves a binary; adding `-S` saves IR instead. Saved output is
  kept on execution failure. Execution still uses the configured runner.
  EMBEDDED disables stdout buffering before JIT initialization so pipe clients
  receive output during execution, including on Windows's shared host CRT.
