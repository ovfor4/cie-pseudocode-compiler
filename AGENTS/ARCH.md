# Architecture

`cpsc` compiles CIE A-Level 9618 pseudocode to LLVM IR. Classic three-stage
Kaleidoscope-style design, C++17, no external deps beyond LLVM 18 and libc.

```
stdin ──> Lexer ──> Parser ──> vector<unique_ptr<StmtAST>> ──> CodeGen ──> textual LLVM IR on stderr
        (getchar)  (recursive       (flat program list;         (one llvm::Module "cps_module",
                    descent)         FunctionDefAST mixed in)    everything else inside i32 main())
```

Fixed value representation (ABI — much code infers types backwards from these):
INTEGER = `i64`, REAL = `double`, BOOLEAN = `i1`, CHAR = `i8`, STRING = opaque `ptr`
to a NUL-terminated buffer.

## Build & run

The compiler stops at textual IR. Making a runnable binary is manual:

```bash
mkdir -p build && cd build && cmake -DLLVM_DIR=/usr/lib/llvm-18/cmake .. && make   # build cpsc
./cpsc < prog.cps 2> prog.ll     # stdin is the ONLY input; IR goes to STDERR
clang-18 prog.ll -o prog && ./prog
```

Facts about the driver (`tools/driver/main.cc`) that break naive automation:

- **No argv.** `./cpsc file.cps` blocks forever on stdin; you must redirect `<`.
- **IR and error messages share stderr.** A failed compile still dumps (partial) IR,
  mixed with `Error:` lines that make clang reject the file.
- **Exit code is non-zero iff a diagnostic was emitted.** Lexer, Parser and CodeGen each
  track an error flag (`hadError()`); every failure path prints an `Error:` line, and
  `main` returns 1 when any flag is set. The emitted module is also checked with
  `verifyModule`, so invalid IR flips the exit code too. Check the exit code — do not
  grep stderr.

Build pin: **LLVM 18 only.** LLVM 19 breaks compilation (`ArithmeticHandler.cc` uses
`llvm::Module` methods relying on LLVM 18's transitive `#include`; fix would be adding
`llvm/IR/Module.h`).

## Layout

| Path | Contents |
|---|---|
| `include/cps/` | All headers, included as `"cps/Foo.h"` |
| `lib/Lexer/Lexer.cc` | Entire lexer: one function, `Lexer::gettok()` |
| `lib/Parser/Parser.cc` | Statements, expressions, precedence table |
| `lib/Parser/ParserFunction.cc` | FUNCTION/PROCEDURE/CALL/RETURN parsing |
| `lib/CodeGen/CodeGen.cc` | Hub: dispatch, coercion, control flow, INPUT/OUTPUT, main() |
| `lib/CodeGen/*Handler.cc`, `FunctionGen.cc`, `RuntimeCheck.cc`, `TypeSystem.cc` | Helpers owned by CodeGen |
| `CMakeLists.txt` | 3 static libs (CPSLexer, CPSParser, CPSCodeGen) + `cpsc`. **New .cc files must be added to the source lists here** |

AST nodes live in `include/cps/AST.h` (+ `FunctionAST.h`); they are pure data with
virtual destructors only — no codegen/visitor methods.

## Detail docs

| File | Read it when |
|---|---|
| `AGENTS/CONVENTIONS.md` | Before writing any code — cross-module invariants whose violation breaks code you didn't touch |
| `AGENTS/EXTENDING.md` | Adding a statement, builtin, operator, type, runtime check, or libc dependency — exact recipes |
| `AGENTS/SUBSYSTEMS.md` | Working on arrays, functions/procedures, or runtime checks |
| `AGENTS/PITFALLS.md` | Verified sharp edges (they *will* bite) and the list of unimplemented 9618 features |
