# Architecture

`cpsc` compiles CIE A-Level 9618 pseudocode to native executables or LLVM IR. Classic three-stage
Kaleidoscope-style design, C++17, built with LLVM 23 and Clang 23. CMake rejects
other major versions and non-Clang compilers. Execution is selected at
build time: external lli, an embedded static LLVM engine, or disabled.

```
file ──> Lexer ──> Parser ──> vector<unique_ptr<StmtAST>> ──> CodeGen ──> LLVM IR ──> NativeCompiler (object emission + embedded LLD)
        (istream)  (recursive       (flat program list;         (one llvm::Module "cps_module",
                    descent)         FunctionDefAST mixed in)    everything else inside i32 main())
```

Fixed value representation (ABI — much code infers types backwards from these):
INTEGER = `i64`, REAL = `double`, BOOLEAN = `i1`, CHAR = `i8`, STRING = opaque `ptr`
to a NUL-terminated buffer. User-defined types (§4): enum = `i64` (0-based ordinal),
pointer = opaque `ptr`, record = named struct `%record.NAME`. Enum/pointer are
LLVM-identical to INTEGER/STRING **by design** — their identity lives in the type
*name*, enforced upstream (see CONVENTIONS.md). Allocation byte sizes are never computed
host-side: the module has no DataLayout string, so every allocation size is a null-GEP
`sizeof` constant (`TypeSystem::getSizeOfConstant`) folded by the backend.

C_INT/C_CHAR/C_STRING/C_DOUBLE alias native int/char/char*/double. C_TYPE string
imports cover standard C integer/real types, _Bool, void and object pointers.
CScalar/CPointer identities are canonical C types, not pseudocode alias names;
native widths and signedness come from sizeof/type traits, while floating formats
come from numeric_limits (long double size alone is insufficient). Programs use
the compiler's build ABI; cross-target emission and EXTERN are not implemented.
CTypeGen handles typed numeric conversions, casts, native arithmetic and I/O.
Crossing C/pseudocode domains requires a cast, including literals. C arithmetic
uses C++ promotions, except that / always returns pseudocode REAL.

## Build & run

The compiler emits native executables by default and textual `.ll` IR with `-S`.
Initial configuration must explicitly select `CPSC_RUN_MODE=PATH`, `EMBEDDED` or
`NONE` (no default). All modes need LLVM/Clang/LLD 23 development libraries,
Python 3.9+ and native C/C++ development files to build; NONE supports both outputs.
The following commands use Debian/Ubuntu's versioned compiler names:

```bash
# Prepare dependencies using the recommendations in doc/build.md.
cmake -S . -B build -DCPSC_RUN_MODE=EMBEDDED \
  -DCMAKE_C_COMPILER=clang-23 -DCMAKE_CXX_COMPILER=clang++-23
cmake --build build
build/cpsc prog.cps              # save prog.out beside the source
build/cpsc prog.cps -S           # save prog.ll beside the source
build/cpsc prog.cps -r           # run without saving output
build/cpsc prog.cps -r -S -o prog.ll # save IR, then run it
build/cpsc prog.cps -o prog && ./prog
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
paths except for supplied compilers. Cached or explicit CMake compiler paths and
initial CC/CXX selections are preserved and validated; default discovery prefers
clang-23/clang++-23, then unversioned clang/clang++. Build tools are resolved before
`project()` enables compiler checks; compiler information is refreshed when the
selected executable changes.
Platform modules relocate package paths only inside the extraction directory;
Homebrew bottles also receive local Cellar/opt links and Mach-O relocation/signing.
Package hashes skip unchanged extractions; a changed package set rebuilds the
managed root. Downloads belong to the explicit dependency commands and static LLD
helper in `doc/build.md`, not to CMake. The Windows Pandoc ZIP is downloaded directly from its official
release because MSYS2 has no Pandoc package. Only the Linux build is verified.

Generated IR uses the C++ exception ABI; native compilation links its runtime
automatically. Manually linking emitted `.ll` files still requires a C++ driver
(use LLVM 23's `clang++`, or `clang++-23` on Debian/Ubuntu).
TRY/CATCH/ENDTRY and bare THROW are compiler extensions.
Runtime checks print the source line and error reason to stdout before throwing
anonymous exceptions; the final handler in generated main
prints `Unhandled exception. Program terminated.` to stdout and returns 1.
Exception execution with the embedded ORC JIT is verified on Linux with LLVM 23;
Windows/macOS await CI verification.

Driver behavior (`tools/driver/main.cc`):

- One source file, with optional `-S`, `-o path` and `-r`; options may precede or follow the source.
  No stdin source input. Missing input, including no arguments, prints
  `cpsc: fatal error: no input files` and `compilation terminated.`, then returns 1.
- `-h` anywhere in the arguments prints one documentation pointer to stdout and
  returns 0 without compiling: `man 1 cpsc` on Unix/macOS, or
  Start Menu > cpsc > cpsc Help on Windows (selected by `_WIN32`).
- Output is written only after successful parsing, code generation and module
  verification. Diagnostics stay on stderr; source errors preserve existing output.
  Native linking uses a staged file beside the output and renames it on success.
- `-S` selects textual IR; otherwise output is a native executable. Without `-o`,
  replace the source extension with `.ll` or `.out` beside the source on every OS.
  `-o` paths are used literally and never select the format. `-r` alone saves nothing;
  either `-S` or `-o` requests saving before execution.
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
  compiling or writing output. The selected `lli --version` must identify LLVM 23
  and succeed within five seconds; other or unrecognized versions fail before
  source compilation. Without `-r`, no external tools are checked; clang/clang++
  are never invoked or checked by this mode. Process-launch/crash failures return 1.
  `-r` alone uses temporary IR removed on return, including execution failure.
- In EMBEDDED mode, `-r` uses LLJIT with JITLink in-process (`tools/driver/EmbeddedRunner.cc`).
  The JIT owns a copy of CodeGen's module made with `cloneExternalModuleToContext`
  into an independent `ThreadSafeContext`; CodeGen retains its module and context.
  The running copy uses the JIT target triple and DataLayout. Static C++ exception
  ABI symbols are defined with ORC `absoluteSymbols` using the JIT mangling
  (MinGW keeps its `snprintf`/`strtoll` aliases and a no-op `__main`); ordinary
  C library symbols resolve through the ORC process-symbol mechanism. Linux uses
  the generic LLVM IR platform with EH-frame registration. macOS uses LLJIT's
  unwind registration, selecting compact unwind or EH-frame per host capability.
  Windows x86_64 keeps the GNU C++ runtime and `__gxx_personality_seh0` with a
  small in-process COFF adapter: per-graph `__ImageBase` shared by relocations
  and SEH registration, preserved `.pdata`/`.xdata` with layout/sort/range checks,
  x86_64 pointer/stub helpers for external/personality calls, and
  `RtlAddFunctionTable`/`RtlDeleteFunctionTable` via JITLink allocation actions
  covering teardown and link-failure cleanup; registration failure blocks entry.
  The JIT explicitly uses `ObjectLinkingLayer` so Windows COFF does not fall back
  to RuntimeDyld. Initialization runs through the platform support before lookup
  of `main`; deinitialization runs while code is still mapped so unwind
  deregistration precedes memory release. Execution failures return 1 without
  `compilation terminated.`. The host target comes from
  `JITTargetMachineBuilder::detectHost()`.
  No external runner or PATH lookup is used; `-r` alone runs in memory.
  Linux embedded execution is verified; Windows/macOS await CI verification.
- In NONE mode, `-r` returns 1 before compiling or writing output; native and IR
  compilation remain available. Only EMBEDDED uses the in-process execution engine.
- Both execution modes return the program's exit status and inherit standard
  streams. `-r -o` saves a binary; adding `-S` saves IR instead. Saved output is
  kept on execution failure. Execution still uses the configured runner.

All modes require static LLVM, Clang Driver and LLD archives. `cmake/EmbeddedLLVM.cmake` also selects
static compression/terminal support archives when LLVM uses them. Linux Clang
builds link the C++ runtime statically; MinGW uses `-static`. The tested Debian
LLVM 23 executable has only libc, libm and the system loader as dynamic dependencies.
macOS links with `-dead_strip_dylibs` to omit unused dylibs inherited
from LLVM's support interface, including Homebrew's Z3. Keep the nightly package's
non-system dependency check; required dylibs are not removed by this option.
Windows and macOS embedded execution await CI verification; Linux EMBEDDED is verified.
On MinGW, `LLVMWindowsManifest` explicitly uses static libxml2, iconv and zlib
plus the system bcrypt library. XML definitions are loaded with `--whole-archive`
to satisfy dllimport references in the prebuilt LLVM archive. `-static` alone
does not replace an imported target's explicit `.dll.a` path.

`NativeCompiler` clones the module, sets the host triple/DataLayout and emits a PIC
object. Clang Driver constructs one platform linker job, which is dispatched to
embedded LLD without launching a process. `cmake/NativeCompiler.cmake` runs
`tools/driver/prepare-native-runtime.py` at configure time to capture the build
toolchain's link inputs and compress them into generated C++ data. Native compilation
extracts startup objects, runtime archives and system link interfaces to a temporary
directory; GNU linker scripts are relocated. macOS embeds SDK library stubs, not
headers or system runtime dylibs. No user development toolchain is required.
Linux statically links non-system runtimes but dynamically links glibc; do not use
global `-static` there. Generated binaries require a compatible host system runtime.

Homebrew's LLD package contains shared libraries. Run `tools/build-static-lld.sh`
with the selected LLVM prefix and cpsc build directory before configuring on macOS.
It builds matching LLD sources, installs static archives and tools into the build
prefix, and leaves LLVM itself installed. Source and build caches are versioned;
the resulting local `lib/cmake/lld` takes precedence over shared system packages.
The helper's build-time linker can use the installed LLVM dylib; cpsc still links
the LLD archives and static LLVM components.
Both macOS configurations explicitly select the SDK with `CMAKE_OSX_SYSROOT`
before compiler detection, so imported SDK include paths do not precede libc++.
The macOS job updates Homebrew and upgrades LLVM 23 before building LLD. It checks
that clangDriver defines `AMDGPU::setArchNameInTriple` to reject older bottles
corrupted by Homebrew's duplicate-member LTO conversion (homebrew-core #301252).

Windows build instructions retain MSYS2 MINGW64. Its LLVM/Clang package names
and compiler executable names are unversioned; CMake still requires major 23.
The build guide explicitly selects `/mingw64/bin/clang.exe` and `clang++.exe`
through `cygpath -m`. MINGW64 retains the GNU C++ runtime; cpsc links with LLD. Switching to
CLANG64 would change the C/C++ runtime and is a separate task.

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
timestamp (`yyyymmdd-hhmmss`) followed by its short ID. Git is only required
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

Build pin: **LLVM 23 and upstream Clang 23** (any 23.x release); 23.1.3 is verified
on Linux. LLVM 23 uses `CreateGlobalString` for opaque string pointers,
`hasTerminator()` for block termination checks (its `getTerminator()` requires an
existing terminator), and an explicit `llvm::Triple` for the embedded target.

## Nightly builds

`.github/workflows/nightly-build.yml` runs on every push to `main`, without path
filters. A shared concurrency group cancels superseded runs. The cleanup job
requires the existing `nightly-build` release and deletes all uploaded assets,
including paginated results, before either build starts. It never creates or
deletes the release and preserves its tag, title and notes. GitHub's generated
source downloads remain available.

The Linux compiler and plugin jobs use Ubuntu 24.04 x86_64; the macOS compiler
job uses a macOS 15 arm64 runner, and Windows uses Windows Server 2025 x86_64.
All four check out the push SHA and run independently after cleanup. All three
compiler jobs use LLVM/Clang/LLD 23, Python, Release and EMBEDDED settings, Pandoc, and the
existing `release` target. Linux obtains its toolchain from apt.llvm.org; macOS
uses Homebrew's LLVM 23 and builds matching static LLD with the helper above. Windows uses `msys2/setup-msys2@v2` with MINGW64,
MSYS2 LLVM/Clang packages, explicit native compiler and LLVM paths, and the
MinGW Makefiles generator. It downloads the official Pandoc 3.12.1 Windows
x86_64 ZIP into `build/deps/packages/` for CMake to extract. Each compiler job
extracts and checks its archive, then runs a small OUTPUT program with the
packaged binary and checks native generation plus `.ll` output without tool lookup. Windows checks its ZIP in PowerShell with PATH limited to
Windows system directories, excluding MSYS2 and LLVM. The macOS check also
verifies arm64 and rejects non-system dynamic libraries. The plugin uses
Node.js 22, `npm ci --ignore-scripts` and
`npm run package`, then checks VSIX contents. Each job uploads directly after
its own checks pass; failure in one does not block the others. Compiler jobs enable
BUILD_TESTING, install Perl and run all CTest cases with the push SHA as seed before
their package smoke checks and uploads. Failed tests retain inputs and logs as
Actions artifacts for 14 days; test summaries appear even on failure. See TESTING.md.
Cleanup and uploads check main's current SHA to avoid modifying nightly for a
superseded push. The workflow uses `GITHUB_TOKEN` with `contents: write`.
Compiler package names retain the existing commit-based version; the VSIX uses
`plugin/package.json`'s version. Before committing workflow changes, run
actionlint, simulate release API calls, and smoke-test the packages locally;
do not modify the real release for local verification.

## Layout

| Path | Contents |
|---|---|
| `include/cps/` | All headers, included as `"cps/Foo.h"` |
| `lib/Lexer/Lexer.cc` | Token scanning and payload-preserving lookahead; `gettok()` consumes and `peekToken`/`peekLine` inspect buffered tokens |
| `lib/Parser/Parser.cc` | Entry point, statement dispatch, assignment parsing and error recovery |
| `lib/Parser/ParserExpression.cc`, `ParserControlFlow.cc` | Expressions/designators and control-flow statement parsing (including ordered CASE branches) |
| `lib/Parser/ParserFunction.cc` | FUNCTION/PROCEDURE/CALL/RETURN + parameter-list parsing |
| `lib/Parser/ParserFile.cc` | OPENFILE/READFILE/WRITEFILE/CLOSEFILE parsing |
| `lib/Parser/ParserType.cc` | Type names, variable/array/constant declarations, TYPE/ENDTYPE and CLASS/ENDCLASS declarations, shared field parsing, SET/DEFINE reserved stubs |
| `lib/CodeGen/CodeGen.cc` | Initialization, symbols, call arguments and main() compilation |
| `lib/CodeGen/ExpressionGen.cc`, `StatementGen.cc`, `ControlFlowGen.cc` | Expression/statement dispatch and control-flow emission |
| `lib/CodeGen/ConstantGen.cc` | Literal constants, declaration conflicts and lexical binding restoration |
| `lib/CodeGen/ValueTypeGen.cc`, `OutputGen.cc`, `Builtins.h` | Expression typing, coercion/declarations, scalar output and the shared builtin table |
| `lib/CodeGen/Designator.cc`, `TypeCheckGen.cc` | TYPE pre-pass and lvalue access; type admission and user-kind binary operators |
| `lib/CodeGen/CTypeGen.cc` | CodeGen methods for native C scalar conversions, casts, arithmetic and input/output |
| `lib/CodeGen/NativeTypeSystem.cc` | Builtin registration, native C type imports/aliases and scalar promotion |
| `lib/CodeGen/FileHandler.cc`, `FileRuntimeIO.cc` | File runtime setup/lookup/open and call entry points; read/write/close/EOF helper bodies |
| `lib/CodeGen/ArrayHandler.cc`, `ArrayParameters.cc`, `ArrayOutput.cc` | Array allocation/indexing, parameter binding/arguments and whole/subarray output |
| `lib/CodeGen/*Handler.cc`, `FunctionGen.cc`, `RuntimeCheck.cc`, `TypeSystem.cc` | Helpers owned by CodeGen (including exception emission) |
| `CMakeLists.txt` | 3 static libs (CPSLexer, CPSParser, CPSCodeGen) + `cpsc`. **New .cc files must be added to the source lists here** |
| `tools/driver/NativeCompiler.cc`, `cmake/NativeCompiler.cmake` | Host object generation, embedded Clang/LLD linking and compressed link resources |
| `tools/driver/EmbeddedRunner.cc`, `cmake/EmbeddedLLVM.cmake` | EMBEDDED-only LLJIT/JITLink runner; static support-library selection for all modes |
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
