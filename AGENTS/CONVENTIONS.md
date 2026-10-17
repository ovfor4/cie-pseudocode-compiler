# Load-bearing conventions

These invariants span modules. Violating any of them breaks code you didn't touch.
(Overview and build instructions: `ARCH.md`.)

**Project text must be in English.** This applies to documentation, source comments,
user-facing messages, examples and test fixtures. Unicode test data must not contain
Chinese text. Keep user instructions in README files concise; put architecture,
implementation notes and AI guidance in the relevant `AGENTS/` directory.

**Tokens are raw `int`.** `Lexer::gettok()` returns negative values for the `Token`
enum (`include/cps/Lexer.h`) and non-negative values for literal ASCII chars (`'('`,
`'+'`, `'<'`). Parser and CodeGen switch on both forms interchangeably; `BinaryExprAST`
stores this same int as its operator. Payloads (identifier text, literal values) arrive
out-of-band via public `Lexer` fields (`IdentifierStr`, `StringVal`, `NumVal`,
`RealVal`, `CharVal`) that must be read before the next `gettok()`.

**Keywords are UPPERCASE and case-sensitive.** `declare` silently lexes as an
identifier. Assignment is `<-` (ASCII only), equality is single `=`, inequality `<>`,
concatenation `&` (reaches the parser as raw ASCII 38 — no token). Identifiers use
ASCII case-insensitive lookup/comparison through `IdentifierMap`, `IdentifierSet`
and `sameIdentifier`; retain source spelling in ASTs and diagnostics. This includes
functions, types, enum values, fields and builtins, but not C_TYPE import strings.
Integer tokens carry a checked unsigned magnitude through lookahead; the parser
applies the sign and i64 range check, including the minimum value. No escape sequences
in string/char literals. Comments are `//` to end of line.

**The lexer reads a caller-owned stream.** Construct it with `Lexer(std::istream &)`;
the stream must outlive the lexer. Lookahead, line number and error state belong to
each instance. The driver opens the source file; stdin is not used for compilation.

**Types travel as uppercase `std::string`** (`"INTEGER"`, `"REAL"`, ..., plus
user-defined names) — never an enum. `TypeSystem` resolves the string to
`TypeInfo{LLVMType, Kind, per-kind payload}`. The parser's `ParseTypeName` accepts *any*
identifier as a type name except `VOID` (rejected outside a RETURNS clause via its
`AllowVoid` flag); other unknown types only fail (softly) in CodeGen.

**C aliases are structural within the C domain.** `CIdentity` retains the canonical
C base type and pointer levels independently of the alias `Name`. Use `sameType`
for storage compatibility (including BYREF and array arguments); never equate C
types just because their LLVM types or widths match. CScalar/CPointer values are
handled by CTypeGen, not the legacy representation coercions. Cross-domain
assignments, arguments and operators require explicit casts, including literals.
Plain C char uses native signedness; pseudocode CHAR stays unsigned. Numeric
conversions use source signedness, and native floating formats use numeric_limits.
Allocation sizes still use LLVM sizeof; native C type widths use build-compiler
sizeof. C alias declarations precede casts using their names; the parser tracks
declared type names to distinguish `(Type)operand` from `(expression)`.

**User-kind typing is nominal, and checked upstream only.** Enum/Record/Pointer
identity is case-insensitive name equality (`P1 = ^INTEGER` and `P2 = ^INTEGER` are incompatible;
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

**Reference storage does not imply address comparison.** Compare values by content;
only explicitly declared pointer types compare addresses. STRING's six comparison
operators use `strcmp` in `StringHandler`, dispatched by pseudocode type in CodeGen
before ArithmeticHandler. Ordering is case-sensitive byte lexicographic order;
mixed STRING/non-STRING comparisons are errors. Whole-array and record comparisons
remain unsupported.

**Strings are malloc'd and never freed — by design.** Every string-producing operation
(`&`, MID/LEFT/RIGHT/LCASE/UCASE, NUM_TO_STR, CHR, READFILE, CHAR/number→STRING
coercion) mallocs a fresh buffer; string literals and BOOLEAN→STRING coercion instead
return pointers to shared module globals, so a STRING value may alias a global. `MemoryHandler` declares `free` for explicit user memory only; it must never free internal STRING buffers. Don't "fix" leaks piecemeal; it's a global design decision.
Heap-string operations live in `StringHandler` (including `&` concatenation,
`emitConcat`), `StringConversionHandler` (NUM_TO_STR/STR_TO_NUM buffers) and
`FileHandler` (READFILE line buffers — its `__cps_file_read` helper grows line buffers with `realloc`); `CodeGen` also keeps its own `MallocFunc` for the char→STRING coercion
and INPUT STRING buffers — declared in `SetupExternalFunctions`, never fetched with a
bare `getFunction("malloc")`.

**One symbol namespace, one table.** `Symbols` (name → `SymbolInfo{Storage,
TypeName, IsArray, ConstantValue, ScopeID}`) in CodeGen is the only symbol table.
A non-null ConstantValue denotes a read-only CONSTANT with no Storage. Read it
through `emitExpr`; all writable/address-taking paths must reject it through
`requireWritable`. Register variables through `registerSymbol()` (parameters are
also bound directly by FunctionGen). `prepareSymbolDeclaration` diagnoses
same-scope conflicts involving constants before declaration emission. Unique
scope IDs distinguish sibling blocks; `emitScopedBlock` restores only bindings
introduced by constants or hiding constants, preserving other variables' existing
function-wide visibility. FunctionGen saves/restores Symbols and inherits only
previously visible top-level constants (ScopeID 0). CodeGen isolates constant
scope bookkeeping and array metadata around each function body. Arrays also
restore metadata at control-flow block exits.

**Dispatch is by `dynamic_cast` chain, not virtual methods.** `CodeGen::emitStmt` and
`emitExpr` walk a linear if-chain of casts. **A statement type missing from the chain is
silently ignored** — no error. Adding an AST node always means touching these chains.

**Compiler error style: print an `error:` line, set the component's error flag, and
keep compiling.** No exceptions, no exit mid-compile. `Lexer`/`Parser`/`CodeGen` each
expose `hadError()`; AST expressions/statements share source-line metadata. Parser
entry points record token locations. CodeGen saves/restores its diagnostic line
across recursive emission and coercion; `reportError` appends `at line N`.
`CodeGen::reportError(fmt, ...)` is the helper for codegen-side
diagnostics (`ArrayHandler` reaches it through its `CodeGen &` parameter,
`FunctionGen`/`ArithmeticHandler` hold a `bool &HadError` reference). The driver exits
non-zero when any flag is set, printing `compilation terminated.` once. Errors that
prevent compilation (missing input, file I/O or invalid IR) use `fatal error:`.
Execution-stage errors do not print the compilation termination line.
The parser drops a failed statement and resyncs at the
next token that can start a statement or end a block (`syncToStatementStart`), then
continues; the failure is always diagnosed and reflected in the exit code.

**Calls share one dispatch path.** `CodeGen::emitCall` handles expression calls, CALL statements and bare `name(args)` statements; statement calls discard results. Explicit casts may bridge C void* and nominal pseudocode pointer types.

**User calls dispatch on the declared signature.** `FunctionGen::emitPrototype` records every
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
(`isReservedRuntimeName`), builtins, or enum values. Signatures retain the first
declaration spelling for LLVM lookup, independently of call-site casing.

**Parameter modes are sticky and BYREF is procedure-only (guide §8.3).** In
`(BYREF a : T, b : T)` the `b` is BYREF too; unannotated parameters default to
BYVAL (§8.1). A BYREF parameter in a FUNCTION is a parse error. **Array
parameters**: `ARRAY OF T` (rank 1, bounds travel with the argument) or
`ARRAY[l:u,...] OF T` (literal bounds, any rank, a contract checked at compile
time when the caller's bounds are constants, else at run time by throwing an exception).
The LLVM ABI flattens each rank-R array parameter into a data `ptr` plus
`kBoundArgsPerDim` (=2) i64 bounds per dimension; the callee rebuilds its
`ArrayMetadata` from those (`ArrayHandler::bindArrayParameter` via the binder
callback CodeGen injects into FunctionGen). BYVAL arrays are whole
malloc+memcpy copies at callee entry; BYREF binds the caller's buffer.

**Builtins are plain identifiers backed by one table.** Builtins
(LENGTH/MID/RIGHT/LEFT/LCASE/UCASE/ASC/CHR/IS_NUM/NUM_TO_STR/STR_TO_NUM/EOF/MALLOC/FREE) parse as
ordinary calls — no lexer/parser support, and the names are not reserved words. The
inline constexpr `Builtins` table in the internal `lib/CodeGen/Builtins.h` ({Name, Arity, ReturnTypeName}) is
the single source of truth: `emitCall` checks arity against it
before dispatching to the per-builtin emit branches (still an if-chain, ahead of the
user-function fallback), and `getExprTypeInfo` reads return types from it. EOF and the memory builtins consume the call source line for runtime diagnostics; they touch mutable runtime state and can throw.

**Program exceptions use the C++ ABI; compiler diagnostics do not.**
`ExceptionHandler` emits anonymous integer exceptions (an internal placeholder),
`invoke`/`landingpad`/`resume`, and paired `__cxa_begin_catch`/`__cxa_end_catch`.
Its handler stack and catch depth are keyed by LLVM function, so synthesizing file
helpers never inherits the caller's catches. User calls, file runtime calls and
THROW must use `ExceptionHandler::emitCall`; ordinary nonthrowing libc calls stay
plain calls. RETURN must finish all active catches after evaluating its value.
`RuntimeCheck::emitCheck` prints the source line and error reason to stdout before
throwing. Generated main catches anything unhandled, prints the generic termination
message to stdout, and returns 1. Explicit bare THROW prints no diagnostic itself.
Arrays in TRY and CATCH use separate block scopes; scalars retain existing function scope.
