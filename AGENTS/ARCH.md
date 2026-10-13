# Architecture

`cpsc` compiles CIE A-Level 9618 pseudocode to LLVM IR. Classic three-stage
Kaleidoscope-style design, C++17, no external deps beyond LLVM 18 and libc.

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

The compiler stops at textual IR. Making a runnable binary is manual:

```bash
mkdir -p build && cd build && cmake -DLLVM_DIR=/usr/lib/llvm-18/cmake .. && make   # build cpsc
./cpsc prog.cps -o prog.ll     # default without -o: prog.ir beside the source
clang-18 prog.ll -o prog && ./prog
```

Driver behavior (`tools/driver/main.cc`):

- One source file, with optional `-o path`; options may precede or follow the source.
  No stdin source input. No arguments exits silently with status 0 (help TODO).
- IR is written only after successful parsing, code generation and module
  verification. Diagnostics stay on stderr; failures do not overwrite existing IR.
- Lexer, Parser and CodeGen track error flags (`hadError()`); any error, including
  invalid IR from `verifyModule`, makes the driver return 1. File/argument errors
  also return 1. Check the exit code rather than grepping stderr.
- Successful compilation overwrites the output, but paths aliasing the source
  (including symlinks/hard links) are rejected.

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

AST nodes live in `include/cps/AST.h` (+ `FunctionAST.h`, `FileAST.h`, `TypeAST.h`);
they are pure data with virtual destructors only — no codegen/visitor methods.

## Detail docs

| File | Read it when |
|---|---|
| `AGENTS/CONVENTIONS.md` | Before writing any code — cross-module invariants whose violation breaks code you didn't touch |
| `AGENTS/EXTENDING.md` | Adding a statement, builtin, operator, type, runtime check, or libc dependency — exact recipes |
| `AGENTS/SUBSYSTEMS.md` | Working on arrays, functions/procedures, or runtime checks |
| `AGENTS/PITFALLS.md` | Verified sharp edges (they *will* bite) and the list of unimplemented 9618 features |
