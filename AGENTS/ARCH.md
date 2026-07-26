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

Three facts about the driver (`tools/driver/main.cc`, 17 lines) that break naive automation:

- **No argv.** `./cpsc file.cps` blocks forever on stdin; you must redirect `<`.
- **IR and error messages share stderr.** A failed parse still dumps (partial) IR,
  prefixed by `Error:` lines that make clang reject the file.
- **Exit code is always 0**, even on errors. Grepping stderr for `Error:` catches most
  failures but has false negatives: some parse paths return nullptr without printing
  anything (an identifier statement not followed by `<-`/`[`, an unrecognized statement
  token), the keyword builtins' arity checks print without the `Error:` prefix
  (`LENGTH expects 1 arg`), and the identifier-route builtins
  (ASC/CHR/IS_NUM/NUM_TO_STR/STR_TO_NUM) fail arity checks silently. Those statements
  just vanish from otherwise-valid IR. The only reliable check is running the program
  and diffing its output.

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
