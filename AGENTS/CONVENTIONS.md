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

**Types travel as uppercase `std::string`** (`"INTEGER"`, `"REAL"`, ..., plus
user-defined names) — never an enum. `TypeSystem` resolves the string to
`TypeInfo{LLVMType, Kind, per-kind payload}`. The parser's `ParseTypeName` accepts *any*
identifier as a type name except `VOID` (rejected outside a RETURNS clause via its
`AllowVoid` flag); other unknown types only fail (softly) in CodeGen.

**User-kind typing is nominal, and checked upstream only.** Enum/Record/Pointer
identity is name equality (`P1 = ^INTEGER` and `P2 = ^INTEGER` are incompatible;
same-layout records too). Their LLVM representations are indistinguishable from
builtins, so values of these kinds must NEVER reach `ArithmeticHandler` or
`coerceValueToType` without a prior name-level check — `CodeGen::emitCoercedExpr`
(assignments, BYVAL args, RETURN, IF/WHILE/REPEAT/FOR operands, file operands,
builtin args) and the binary-op interception at the top of `emitExpr`'s
`BinaryExprAST` branch are the only doors. `coerceValueToType` hard-errors
("Internal: ...") if a user-kind target reaches it.

**Enum value names are a flat, globally reserved namespace** owned by `TypeSystem`
(not `Symbols`, so they survive FunctionGen's save/clear/restore). Expression
resolution order is `Symbols` first, then enum constants; collisions with
DECLARE/array/parameter/function names are compile errors at declaration time,
so the order can never shadow. TYPE declarations are top-level only, registered
by `compile()`'s two-pass pre-scan (names first — pointer pointees may
forward-reference; definitions second, in source order — a by-value record
field requires its record type to be already complete).

**Designators degrade.** The parser turns `x` / `arr[i]` into the legacy
`VariableExprAST`/`ArrayAccessExprAST` (and their assign statements) and only
builds `DesignatorExprAST`/`DesignatorAssignStmtAST` for chains involving `.`,
`^` or multiple accessors — existing programs keep byte-identical ASTs.
`CodeGen::emitLValue` resolves any of these to `{address, type name}`, emitting
index and null-deref checks on the way; `loadFromLValue` owns the rvalue read
and the STRING null→`""` guard.

**Fixed IR representation per type.** INTEGER = `i64`, REAL = `double`, BOOLEAN = `i1`,
CHAR = `i8`, STRING = opaque `ptr` to a NUL-terminated buffer. Some code still infers
pseudocode types *backwards* from LLVM types (`getExprTypeInfo`'s fallback,
`ArithmeticHandler`'s `isIntegerTy(64)` tests), so these widths are ABI.

**Strings are malloc'd and never freed — by design.** Every string-producing operation
(`&`, MID/LEFT/RIGHT/LCASE/UCASE, NUM_TO_STR, CHR, READFILE, CHAR/number→STRING
coercion) mallocs a fresh buffer; string literals and BOOLEAN→STRING coercion instead
return pointers to shared module globals, so a STRING value may alias a global. `free`
is never declared or called. Don't "fix" leaks piecemeal; it's a global design decision.
Heap-string operations live in `StringHandler` (including `&` concatenation,
`emitConcat`), `StringConversionHandler` (NUM_TO_STR/STR_TO_NUM buffers) and
`FileHandler` (READFILE line buffers — its `__cps_file_read` helper owns the project's
only `realloc`); `CodeGen` also keeps its own `MallocFunc` for the char→STRING coercion
and INPUT STRING buffers — declared in `SetupExternalFunctions`, never fetched with a
bare `getFunction("malloc")`.

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
function's pseudocode signature (`FuncSig`: return type name + per-param `ParamSig`
{type name, mode, array rank/bounds}); `CodeGen::marshalCallArgs` — the single
call-marshalling path for both call-expr and call-stmt — requires BYREF arguments to be
bare variables of *exactly* the declared type (their alloca is passed), admits BYVAL
arguments through `emitCoercedExpr`, and dispatches whole-array arguments to
`ArrayHandler::emitArrayArgument`. RETURN values go through `emitCoercedExpr` against
the declared return type. Top-level prototypes are pre-registered (after the TYPE
pre-pass) before statement emission, so call-before-definition is fine; a call with
no registered signature is a compile error. Function names get no mangling, so
`emitPrototype` rejects names that collide with runtime/libc symbols
(`isReservedRuntimeName`), builtins, or enum values.

**Parameter modes are sticky and BYREF is procedure-only (guide §8.3).** In
`(BYREF a : T, b : T)` the `b` is BYREF too; unannotated parameters default to
BYVAL (§8.1). A BYREF parameter in a FUNCTION is a parse error. **Array
parameters**: `ARRAY OF T` (rank 1, bounds travel with the argument) or
`ARRAY[l:u,...] OF T` (literal bounds, any rank, a contract checked at compile
time when the caller's bounds are constants, else at run time with `[Fatal]`).
The LLVM ABI flattens each rank-R array parameter into a data `ptr` plus
`kBoundArgsPerDim` (=2) i64 bounds per dimension; the callee rebuilds its
`ArrayMetadata` from those (`ArrayHandler::bindArrayParameter` via the binder
callback CodeGen injects into FunctionGen). BYVAL arrays are whole
malloc+memcpy copies at callee entry; BYREF binds the caller's buffer.

**Builtins are plain identifiers backed by one table.** All 12 builtins
(LENGTH/MID/RIGHT/LEFT/LCASE/UCASE/ASC/CHR/IS_NUM/NUM_TO_STR/STR_TO_NUM/EOF) parse as
ordinary calls — no lexer/parser support, and the names are not reserved words. The
static `Builtins` table at the top of `CodeGen.cc` ({Name, Arity, ReturnTypeName}) is
the single source of truth: `emitExpr`'s `CallExprAST` branch checks arity against it
before dispatching to the per-builtin emit branches (still an if-chain, ahead of the
user-function fallback), and `getExprTypeInfo` reads return types from it. EOF is the
odd one out: its intercept consumes `CallExprAST::getLine()` (runtime diagnostics) and
it is the only builtin that touches mutable runtime state and can exit(1) at run time.
