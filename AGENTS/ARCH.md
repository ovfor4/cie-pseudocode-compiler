# Architecture

`cpsc` compiles CIE A-Level 9618 pseudocode to LLVM IR. Classic three-stage
Kaleidoscope-style design, C++17, built with LLVM 18. Execution is selected at
build time: external lli, an embedded static LLVM engine, or disabled.

```
file ──> Lexer ──> Parser ──> vector<unique_ptr<StmtAST>> ──> CodeGen ──> textual LLVM IR in a file
        (istream)  (recursive       (flat program list;         (one llvm::Module "cps_module",
                    descent)         FunctionDefAST mixed in)    everything else inside i32 main())
```

Fixed value representation (ABI — much code infers types backwards from these):
INTEGER = `i64`, REAL = `double`, BOOLEAN = `i1`, CHAR = `i8`, STRING = opaque `ptr`
to a NUL-terminated buffer. User-defined types (§4): enum = `i64` (0-based ordinal),
pointer = opaque `ptr`, record = named struct `%record.NAME`. Enum/pointer are
LLVM-identical to INTEGER/STRING **by design** — their identity lives in the type
*name*, enforced upstream (see CONVENTIONS.md). Byte sizes are never computed
host-side: the module has no DataLayout string, so every size is a null-GEP
`sizeof` constant (`TypeSystem::getSizeOfConstant`) folded by the backend.

## Build & run

The compiler emits textual IR. Initial configuration must explicitly select
`CPSC_RUN_MODE=PATH`, `EMBEDDED` or `NONE` (no default). All modes need LLVM
development libraries to build; NONE still generates IR. Native binaries remain manual:

```bash
# Prepare dependencies using the recommendations in doc/build.md.
cmake -S . -B build -DCPSC_RUN_MODE=EMBEDDED \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
build/cpsc prog.cps -o prog.ll # default without -o: prog.ir beside the source
build/cpsc prog.cps -r         # execute using the selected build mode
build/cpsc prog.cps -r -o prog.ir # save IR, then run it
clang++-18 prog.ll -o prog && ./prog
```

`doc/build.md` recommends which packages to install and which to download;
CMake does not enforce that split. The modular `cmake/LocalDependencies.cmake`
entry point extracts Debian, MSYS2 and Homebrew binary packages from the build
directory's `deps/packages/` into `deps/root/`. Standard bin/lib/include layouts
under the build directory are also searched. The shared
`cmake/FindDependency.cmake` helper searches locally first, then the system, and
reports missing required dependencies. Compilers, make, LLVM, support libraries,
Pandoc, Git and installer/relocation tools use the same lookup policy. System-only
and mixed installations are supported. Local dependencies override cached system
paths. Build tools are resolved before `project()` enables compiler checks;
compiler information is refreshed when the selected executable changes.
Platform modules relocate package paths only inside the extraction directory;
Homebrew bottles also receive local Cellar/opt links and Mach-O relocation/signing.
Package hashes skip unchanged extractions; a changed package set rebuilds the
managed root. Downloads belong to the package-manager commands in `doc/build.md`,
not to CMake. The Windows Pandoc ZIP is downloaded directly from its official
release because MSYS2 has no Pandoc package. Only the Linux build is verified.

Generated IR uses the C++ exception ABI; native binaries must link its runtime
(use `clang++-18`). TRY/CATCH/ENDTRY and bare THROW are compiler extensions.
Runtime checks throw anonymous exceptions; the final handler in generated main
prints `Unhandled exception. Program terminated.` to stdout and returns 1.
Exception execution is verified on Linux with LLVM 18; Windows/macOS are unverified.

Driver behavior (`tools/driver/main.cc`):

- One source file, with optional `-o path` and `-r`; options may precede or follow the source.
  No stdin source input. Missing input, including no arguments, prints
  `cpsc: fatal error: no input files` and `compilation terminated.`, then returns 1.
- `-h` anywhere in the arguments prints one documentation pointer to stdout and
  returns 0 without compiling: `man 1 cpsc` on Unix/macOS, or
  Start Menu > cpsc > cpsc Help on Windows (selected by `_WIN32`).
- IR is written only after successful parsing, code generation and module
  verification. Diagnostics stay on stderr; failures do not overwrite existing IR.
- Lexer, Parser and CodeGen track error flags (`hadError()`); any error, including
  invalid IR from `verifyModule`, makes the driver return 1. File/argument errors
  also return 1. Ordinary compiler errors use `error:`; failures that prevent
  compilation, such as file I/O errors, use `fatal error:`. The driver prints
  `compilation terminated.` once on compilation failure, including argument errors.
  Execution-stage failures do not print that line. Check the exit code rather
  than grepping stderr.
- Successful compilation overwrites the output, but paths aliasing the source
  (including symlinks/hard links) are rejected.
- In PATH mode, `-r` executes PATH's `lli` using LLVM Support process APIs, with
  inherited stdin/stdout/stderr and no shell. Missing lli is rejected before
  compiling or writing output. Process-launch/crash failures return 1. `-r`
  alone uses temporary IR removed on return, including execution failure.
- In EMBEDDED mode, `-r` uses MCJIT in-process (`tools/driver/EmbeddedRunner.cc`).
  The engine owns a clone of CodeGen's module; CodeGen retains the context and
  must outlive the engine. Static C++ exception ABI symbols are explicitly
  registered for JIT lookup; MCJIT registers the generated unwind frames.
  The host target is initialized only for execution.
  No external runner or PATH lookup is used; `-r` alone runs in memory.
- In NONE mode, `-r` returns 1 before compiling or writing output; ordinary IR
  compilation remains available. Only EMBEDDED links execution-engine components.
- Both execution modes return the program's exit status and inherit standard
  streams. `-r -o` saves IR before executing and keeps it on execution failure.

EMBEDDED requires static LLVM archives. `cmake/EmbeddedLLVM.cmake` also selects
static compression/terminal support archives when LLVM uses them. Linux GCC/Clang
builds link the C++ runtime statically; MinGW uses `-static`. The tested Debian
LLVM 18 executable has only libc, libm and the system loader as dynamic dependencies,
and runs in a minimal chroot without LLVM/lli. Windows EMBEDDED is untested.

On Linux, `make -C build install` builds the manual and calls the POSIX
`install.sh` to copy `cpsc` into `CMAKE_INSTALL_PREFIX/bin` and all manuals into
`CMAKE_INSTALL_PREFIX/share/cpsc/man/man1` and `man7` (default prefix `/usr/local`).
Linux man-db registration appends a deduplicated MANDATORY_MANPATH entry to
`/etc/manpath.config` or `/etc/man_db.conf` as root, or `~/.manpath` as a user.
macOS root registration uses `/usr/local/etc/man.d/cpsc.conf`; user installation
adds a MANPATH block to zsh/bash login configuration and prints optional sudo
commands for global registration. Existing configuration is preserved. Explicit
MANPATH settings may need updating; Linux registration rejects whitespace in the
prefix, and Unix prefixes cannot contain colons/newlines.
The script also supports `--prefix`, `--source-dir` and `DESTDIR` staging;
staging never changes host manpath or shell configuration. On macOS it removes
`com.apple.quarantine` from the installed executable if present; attribute inspection
or removal failures abort installation with a diagnostic. Ordinary builds
do not require Pandoc; `doc`, installation and release packaging do. The modular
`cmake/Manual.cmake` target defers Pandoc discovery until manual generation, prefers
the extracted executable, and sets its local runtime/data paths as needed. System
Pandoc remains the fallback.
`make -C build release` packages the executable, all manuals, installer and license
as `build/cpsc-VERSION-SYSTEM-ARCH.tar.xz`, retaining the configured execution mode.
`cmake/Release.cmake` reads HEAD at packaging time; VERSION is the commit's UTC
timestamp (`yyyy-mm-dd-hh-mm-ss`) followed by its short ID. Git is only required
for packaging; recipients can install the extracted archive using just the script.

The Windows `install.ps1` installs `cpsc.exe`, `doc/cpsc.html` and
`doc/language/**/*.html` for the current user under `%LOCALAPPDATA%\Programs\cpsc` (or `-Prefix`). It appends that directory
to the user PATH without copying the combined process PATH, and creates a
per-user Start Menu help shortcut. `-SourceDir` selects the artifact directory;
`DESTDIR` staging is unsupported on Windows. The script is compatible with
Windows PowerShell 5.1; its Windows actions have only been reviewed statically.
The `doc` target uses CMake's `WIN32` target-platform detection: Windows generates
standalone HTML with inline styles at `build/doc/cpsc.html`; other platforms
generate `build/doc/cpsc.1`. The section 1 source is `doc/cpsc.1.md`; section 7 sources are
`doc/language/**/*.7.md`. `doc` builds every page, preserving the language
category subdirectories, as `.7` on Unix or `.html` on Windows. Sources are discovered with
CONFIGURE_DEPENDS; per-page commands regenerate only changed pages.
`cmake/ManualLinks.lua` renders cross-references as man names or local HTML links.
Windows `mingw32-make -C build install` builds the HTML manual and invokes
`install.ps1` through Windows PowerShell with the build directory and
`CMAKE_INSTALL_PREFIX`. The default Windows prefix is the per-user Programs
directory above; explicit prefixes are preserved. Unix still invokes `install.sh`.
The release target explicitly passes the configured Windows flag to
`cmake/Release.cmake`, since a standalone CMake script otherwise describes the
host platform. Windows packages `cpsc.exe`, `doc/cpsc.html`, `doc/language/**/*.html`, `install.ps1` and
`LICENSE` as `build/cpsc-VERSION-Windows-ARCH.zip` using CMake's default Deflate
compression. Unix retains `.tar.xz`, the POSIX installer and the man pages.
Linux smoke tests can configure an isolated build with `-DWIN32=TRUE` to exercise
HTML/ZIP packaging; the resulting renamed Linux executable is only a test fixture,
not a usable Windows release.

Build pin: developed and tested against **LLVM 18**. (The historical LLVM 19 breakage —
`ArithmeticHandler.cc` touching `llvm::Module` through a transitive include — is gone
now that string concatenation lives in `StringHandler`, but newer LLVM versions remain
untested.)

## Layout

| Path | Contents |
|---|---|
| `include/cps/` | All headers, included as `"cps/Foo.h"` |
| `lib/Lexer/Lexer.cc` | Entire lexer: one function, `Lexer::gettok()` |
| `lib/Parser/Parser.cc` | Statements, expressions, designators, precedence table |
| `lib/Parser/ParserFunction.cc` | FUNCTION/PROCEDURE/CALL/RETURN + parameter-list parsing |
| `lib/Parser/ParserFile.cc` | OPENFILE/READFILE/WRITEFILE/CLOSEFILE parsing |
| `lib/Parser/ParserType.cc` | TYPE/ENDTYPE and CLASS/ENDCLASS declarations, shared field parsing, SET/DEFINE reserved stubs |
| `lib/CodeGen/CodeGen.cc` | Hub: dispatch, coercion, control flow, INPUT/OUTPUT, main() |
| `lib/CodeGen/Designator.cc` | CodeGen methods for the TYPE system: type pre-pass, `emitLValue`/`loadFromLValue`, `emitCoercedExpr` gate, user-kind binary-op interception |
| `lib/CodeGen/*Handler.cc`, `FunctionGen.cc`, `RuntimeCheck.cc`, `TypeSystem.cc` | Helpers owned by CodeGen (including exception emission) |
| `CMakeLists.txt` | 3 static libs (CPSLexer, CPSParser, CPSCodeGen) + `cpsc`. **New .cc files must be added to the source lists here** |
| `tools/driver/EmbeddedRunner.cc`, `cmake/EmbeddedLLVM.cmake` | EMBEDDED-only MCJIT runner and static support-library selection |
| `cmake/LocalDependencies.cmake`, `cmake/*Packages.cmake` | Local package extraction and platform relocation |
| `cmake/DependencyPaths.cmake`, `cmake/FindDependency.cmake` | Shared local paths and local-first/system-fallback lookup |
| `cmake/BuildTools.cmake`, `cmake/LLVMDependencies.cmake` | Compiler/build tool selection and LLVM dependency discovery |
| `cmake/Manual.cmake`, `cmake/GenerateManual.cmake.in` | Optional manual generation with local-first Pandoc discovery |
| `install.sh`, `install.ps1`, `cmake/Install.cmake.in`, `cmake/Release.cmake` | Artifact installation and versioned binary release packaging |

AST nodes live in `include/cps/AST.h` (+ `FunctionAST.h`, `FileAST.h`, `TypeAST.h`);
they are pure data with virtual destructors only — no codegen/visitor methods.

## Detail docs

| File | Read it when |
|---|---|
| `AGENTS/CONVENTIONS.md` | Before writing any code — cross-module invariants whose violation breaks code you didn't touch |
| `AGENTS/EXTENDING.md` | Adding a statement, builtin, operator, type, runtime check, or libc dependency — exact recipes |
| `AGENTS/SUBSYSTEMS.md` | Working on arrays, functions/procedures, or runtime checks |
| `AGENTS/PITFALLS.md` | Verified sharp edges (they *will* bite) and the list of unimplemented 9618 features |
| `AGENTS/MAN.md` | Editing man Markdown, language manual layout, generation and installation |

## Language manuals

See [MAN.md](MAN.md) before editing the command or syntax manuals. It defines
source layout, naming, prose formatting and the small checks needed for changes.
