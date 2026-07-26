# Load-bearing conventions

These invariants span modules. Violating any of them breaks code you didn't touch.
(Overview and build instructions: `ARCH.md`.)

**Tokens are raw `int`.** `Lexer::gettok()` returns negative values for the `Token`
enum (`include/cps/Lexer.h`) and non-negative values for literal ASCII chars (`'('`,
`'+'`, `'<'`). Parser and CodeGen switch on both forms interchangeably; `BinaryExprAST`
stores this same int as its operator. Payloads (identifier text, literal values) arrive
out-of-band via public `Lexer` fields (`IdentifierStr`, `StringVal`, `NumVal`,
`RealVal`, `CharVal`) that must be read before the next `gettok()`.

**Keywords are UPPERCASE and case-sensitive.** `declare` silently lexes as an
identifier. Assignment is `<-` (ASCII only), equality is single `=`, inequality `<>`,
concatenation `&` (reaches the parser as raw ASCII 38 — no token). No escape sequences
in string/char literals. Comments are `//` to end of line.

**The lexer can run once per process, from stdin only.** `gettok()` keeps its lookahead
in a function-local `static int LastChar` and calls `getchar()` directly. No re-lexing,
no file paths, and unit tests can't lex two programs in one process.

**Types travel as uppercase `std::string`** (`"INTEGER"`, `"REAL"`, `"BOOLEAN"`,
`"CHAR"`, `"STRING"`, `"VOID"`) — never an enum. `TypeSystem` resolves the string to
`TypeInfo{LLVMType, ElementSize, Kind, ...}`. The parser's `ParseTypeName` accepts *any*
identifier as a type name except `VOID` (rejected outside a RETURNS clause via its
`AllowVoid` flag); other unknown types only fail (softly) in CodeGen.

**Fixed IR representation per type.** INTEGER = `i64`, REAL = `double`, BOOLEAN = `i1`,
CHAR = `i8`, STRING = opaque `ptr` to a NUL-terminated buffer. Much code infers
pseudocode types *backwards* from LLVM types (`getExprTypeInfo`, `ArithmeticHandler`'s
`isIntegerTy(64)` tests, BYREF detection), so these widths are ABI.

**Strings are malloc'd and never freed — by design.** Every string-producing operation
(`&`, MID/LEFT/RIGHT/LCASE/UCASE, NUM_TO_STR, CHR, CHAR/number→STRING coercion) mallocs
a fresh buffer; string literals and BOOLEAN→STRING coercion instead return pointers to
shared module globals, so a STRING value may alias a global. `free` is declared but has
zero call sites. Don't "fix" leaks piecemeal; it's a global design decision.

**One flat symbol namespace.** Two parallel maps in CodeGen — `NamedValues`
(name → storage `Value*`) and `Symbols` (name → `SymbolInfo{Storage, TypeName,
IsArray}`) — must always be updated in lock-step: via `registerSymbol()`, or directly
but both together as `FunctionGen::createArgumentAllocas` does for parameters. (The
per-type handlers' `emitDeclare` methods set `NamedValues` alone — that is dead code;
don't imitate it.) There is no scope stack. `FunctionGen` saves/clears/restores both
maps around a function body, but `ArrayHandler::ArrayTable` is *not* scoped (see
`PITFALLS.md`).

**Dispatch is by `dynamic_cast` chain, not virtual methods.** `CodeGen::emitStmt` and
`emitExpr` walk a linear if-chain of casts. **A statement type missing from the chain is
silently ignored** — no error. Adding an AST node always means touching these chains.

**Error style everywhere: print an `Error:` line, set the component's error flag, and
keep compiling.** No exceptions, no exit mid-compile. `Lexer`/`Parser`/`CodeGen` each
expose `hadError()`; `CodeGen::reportError(fmt, ...)` is the helper for codegen-side
diagnostics (`ArrayHandler` reaches it through its `CodeGen &` parameter,
`FunctionGen`/`ArithmeticHandler` hold a `bool &HadError` reference). The driver exits
non-zero when any flag is set. The parser still drops a failed statement, skips one
token, and continues — later statements can be swallowed during recovery, but the
failure is always diagnosed and reflected in the exit code.

**BYREF is a pointer-typed parameter, inferred structurally at call sites.**
`FunctionGen` emits pointer params for BYREF; call sites (in CodeGen, duplicated for
call-expr and call-stmt) treat *any* pointer-typed callee param as BYREF and require the
argument to be a bare variable whose alloca is passed. The callee side, however, binds
by the *declared* flag. Since STRING itself lowers to `ptr`, the two sides disagree for
STRING: the caller always passes the alloca, but a declared-BYVAL STRING param (the
default) copies that alloca *address* and treats it as the string data pointer — reading
garbage at runtime. Only declared-BYREF STRING parameters work, and STRING arguments can
never be literals/expressions. Function names get no mangling — a pseudocode
`FUNCTION printf` collides with libc.

**Builtins are plain identifiers backed by one table.** All 11 builtins
(LENGTH/MID/RIGHT/LEFT/LCASE/UCASE/ASC/CHR/IS_NUM/NUM_TO_STR/STR_TO_NUM) parse as
ordinary calls — no lexer/parser support, and the names are not reserved words. The
static `Builtins` table at the top of `CodeGen.cc` ({Name, Arity, ReturnTypeName}) is
the single source of truth: `emitExpr`'s `CallExprAST` branch checks arity against it
before dispatching to the per-builtin emit branches (still an if-chain, ahead of the
user-function fallback), and `getExprTypeInfo` reads return types from it.
