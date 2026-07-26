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
CHAR = `i8`, STRING = opaque `ptr` to a NUL-terminated buffer. Some code still infers
pseudocode types *backwards* from LLVM types (`getExprTypeInfo`'s fallback,
`ArithmeticHandler`'s `isIntegerTy(64)` tests), so these widths are ABI.

**Strings are malloc'd and never freed — by design.** Every string-producing operation
(`&`, MID/LEFT/RIGHT/LCASE/UCASE, NUM_TO_STR, CHR, CHAR/number→STRING coercion) mallocs
a fresh buffer; string literals and BOOLEAN→STRING coercion instead return pointers to
shared module globals, so a STRING value may alias a global. `free` is never declared or
called. Don't "fix" leaks piecemeal; it's a global design decision. Heap-string
operations live in `StringHandler` (including `&` concatenation, `emitConcat`) and
`StringConversionHandler` (NUM_TO_STR/STR_TO_NUM buffers); `CodeGen` also keeps its own
`MallocFunc` for the char→STRING coercion and INPUT STRING buffers — declared in
`SetupExternalFunctions`, never fetched with a bare `getFunction("malloc")`.

**One flat symbol namespace, one table.** `Symbols` (name → `SymbolInfo{Storage,
TypeName, IsArray}`) in CodeGen is the only symbol table; register through
`registerSymbol()` (or directly, as `FunctionGen::createArgumentAllocas` does for
parameters) and read storage through `getNamedValue()`/`getSymbolInfo()`. There is no
scope stack. `FunctionGen` saves/clears/restores the table around a function body, but
`ArrayHandler::ArrayTable` is *not* scoped (see `PITFALLS.md`).

**Dispatch is by `dynamic_cast` chain, not virtual methods.** `CodeGen::emitStmt` and
`emitExpr` walk a linear if-chain of casts. **A statement type missing from the chain is
silently ignored** — no error. Adding an AST node always means touching these chains.

**Error style everywhere: print an `Error:` line, set the component's error flag, and
keep compiling.** No exceptions, no exit mid-compile. `Lexer`/`Parser`/`CodeGen` each
expose `hadError()`; `CodeGen::reportError(fmt, ...)` is the helper for codegen-side
diagnostics (`ArrayHandler` reaches it through its `CodeGen &` parameter,
`FunctionGen`/`ArithmeticHandler` hold a `bool &HadError` reference). The driver exits
non-zero when any flag is set. The parser drops a failed statement and resyncs at the
next token that can start a statement or end a block (`syncToStatementStart`), then
continues; the failure is always diagnosed and reflected in the exit code.

**Calls dispatch on the declared signature.** `FunctionGen::emitPrototype` records every
function's pseudocode signature (`FuncSig`: return type name + per-param type name and
BYREF flag); `CodeGen::marshalCallArgs` — the single call-marshalling path for both
call-expr and call-stmt — requires BYREF arguments to be bare variables of *exactly* the
declared type (their alloca is passed), coerces BYVAL arguments to the declared
parameter type, and rejects whole-array arguments. RETURN values are coerced to the
declared return type. STRING works on both sides in both modes. Top-level prototypes are
pre-registered before statement emission, so call-before-definition is fine; a call with
no registered signature is a compile error. Function names get no mangling, so
`emitPrototype` rejects names that collide with runtime/libc symbols
(`isReservedRuntimeName`) or with builtins.

**Builtins are plain identifiers backed by one table.** All 11 builtins
(LENGTH/MID/RIGHT/LEFT/LCASE/UCASE/ASC/CHR/IS_NUM/NUM_TO_STR/STR_TO_NUM) parse as
ordinary calls — no lexer/parser support, and the names are not reserved words. The
static `Builtins` table at the top of `CodeGen.cc` ({Name, Arity, ReturnTypeName}) is
the single source of truth: `emitExpr`'s `CallExprAST` branch checks arity against it
before dispatching to the per-builtin emit branches (still an if-chain, ahead of the
user-function fallback), and `getExprTypeInfo` reads return types from it.
