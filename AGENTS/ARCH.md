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
mkdir -p build && cd build
cmake -DCPSC_RUN_MODE=EMBEDDED -DLLVM_DIR=/usr/lib/llvm-18/cmake .. && make
./cpsc prog.cps -o prog.ll     # default without -o: prog.ir beside the source
./cpsc prog.cps -r             # execute using the selected build mode
./cpsc prog.cps -r -o prog.ir  # save IR, then run it
clang-18 prog.ll -o prog && ./prog
```

Driver behavior (`tools/driver/main.cc`):

- One source file, with optional `-o path` and `-r`; options may precede or follow the source.
  No stdin source input. No arguments exits silently with status 0 (help TODO).
- IR is written only after successful parsing, code generation and module
  verification. Diagnostics stay on stderr; failures do not overwrite existing IR.
- Lexer, Parser and CodeGen track error flags (`hadError()`); any error, including
  invalid IR from `verifyModule`, makes the driver return 1. File/argument errors
  also return 1. Check the exit code rather than grepping stderr.
- Successful compilation overwrites the output, but paths aliasing the source
  (including symlinks/hard links) are rejected.
- In PATH mode, `-r` executes PATH's `lli` using LLVM Support process APIs, with
  inherited stdin/stdout/stderr and no shell. Missing lli is rejected before
  compiling or writing output. Process-launch/crash failures return 1. `-r`
  alone uses temporary IR removed on return, including execution failure.
- In EMBEDDED mode, `-r` uses MCJIT in-process (`tools/driver/EmbeddedRunner.cc`).
  The engine owns a clone of CodeGen's module; CodeGen retains the context and
  must outlive the engine. The host target is initialized only for execution.
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
| `lib/Parser/ParserType.cc` | TYPE/ENDTYPE declarations, SET/DEFINE reserved stubs |
| `lib/CodeGen/CodeGen.cc` | Hub: dispatch, coercion, control flow, INPUT/OUTPUT, main() |
| `lib/CodeGen/Designator.cc` | CodeGen methods for the TYPE system: type pre-pass, `emitLValue`/`loadFromLValue`, `emitCoercedExpr` gate, user-kind binary-op interception |
| `lib/CodeGen/*Handler.cc`, `FunctionGen.cc`, `RuntimeCheck.cc`, `TypeSystem.cc` | Helpers owned by CodeGen |
| `CMakeLists.txt` | 3 static libs (CPSLexer, CPSParser, CPSCodeGen) + `cpsc`. **New .cc files must be added to the source lists here** |
| `tools/driver/EmbeddedRunner.cc`, `cmake/EmbeddedLLVM.cmake` | EMBEDDED-only MCJIT runner and static support-library selection |

AST nodes live in `include/cps/AST.h` (+ `FunctionAST.h`, `FileAST.h`, `TypeAST.h`);
they are pure data with virtual destructors only — no codegen/visitor methods.

## Detail docs

| File | Read it when |
|---|---|
| `AGENTS/CONVENTIONS.md` | Before writing any code — cross-module invariants whose violation breaks code you didn't touch |
| `AGENTS/EXTENDING.md` | Adding a statement, builtin, operator, type, runtime check, or libc dependency — exact recipes |
| `AGENTS/SUBSYSTEMS.md` | Working on arrays, functions/procedures, or runtime checks |
| `AGENTS/PITFALLS.md` | Verified sharp edges (they *will* bite) and the list of unimplemented 9618 features |
