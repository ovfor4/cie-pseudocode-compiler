# Architecture

`cpsc` compiles CIE A-Level 9618 pseudocode to native executables or LLVM IR (C++17, LLVM 23 / Clang 23).
This file is an index; the detail lives in `AGENTS/ARCH/`.

| File | Contents |
|---|---|
| [OVERVIEW.md](ARCH/OVERVIEW.md) | Three-stage pipeline, fixed value representation (ABI), C interop types, pointer to language manuals |
| [BUILD.md](ARCH/BUILD.md) | Build and run commands, run modes, exception ABI, runtime errors, CI overview |
| [DEPENDENCIES.md](ARCH/DEPENDENCIES.md) | Local/system dependency lookup, compiler selection, MSYS2 notes, LLVM 23 pin |
| [DRIVER.md](ARCH/DRIVER.md) | `tools/driver/main.cc`: options, multi-file inputs, entry points, output and exit-code rules, PATH/NONE modes |
| [EMBEDDED_RUNNER.md](ARCH/EMBEDDED_RUNNER.md) | EMBEDDED LLJIT/JITLink runner, MinGW COFF/SEH support, archive loading |
| [LINKING.md](ARCH/LINKING.md) | Static LLVM/LLD linking, `NativeCompiler`, macOS static LLD helper and its CI caching |
| [INSTALL.md](ARCH/INSTALL.md) | `install.sh`/`install.ps1`, manual generation, release packaging |
| [NIGHTLY.md](ARCH/NIGHTLY.md) | `nightly-build.yml` jobs, Bundle packages, publish rules, package naming |
| [LAYOUT.md](ARCH/LAYOUT.md) | Source file map (lexer, parser, codegen, CMake, installers), AST conventions |
| [LIBRARY_UNITS.md](ARCH/LIBRARY_UNITS.md) | `#LIB` library units and the embedded standard library |

## Detail docs

| File | Read it when |
|---|---|
| `AGENTS/CONVENTIONS.md` | Before writing any code — cross-module invariants whose violation breaks code you didn't touch |
| `AGENTS/EXTENDING.md` | Adding a statement, builtin, operator, type, runtime check, or libc dependency — exact recipes |
| `AGENTS/SUBSYSTEMS.md` | Working on arrays, functions/procedures, or runtime checks |
| `AGENTS/PITFALLS.md` | Verified sharp edges (they *will* bite) and the list of unimplemented 9618 features |
| `AGENTS/MAN.md` | Editing man Markdown, language manual layout, generation and installation |
