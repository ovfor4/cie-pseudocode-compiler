# Subsystem notes

Deep-dives on the CodeGen helper subsystems. (Invariants: `CONVENTIONS.md`;
known bugs: `PITFALLS.md`.)

**Arrays** (`ArrayHandler`): a DECLAREd array is one malloc'd flat buffer (memset to
zero), row-major, any rank, any bounds. Bounds and multipliers are *SSA values* cached
in `ArrayMetadata`; `ArrayTable` is scoped per function (`exchangeTable`, driven from
CodeGen's `FunctionDefAST` branch, mirroring the Symbols discipline). Element sizes are
`sizeof` constants (`ElementSizeC`), never host-side numbers. Every access goes through
`emitCheckedIndices` (validates the index count for the caller's semantics, then
per-dimension coerce + `emitIndexCheck`) → `computeFlatIndex` → `loadElement` (load +
the STRING null-guard); the public `emitElementAddress` exposes the same checked
pipeline as an address for the lvalue layer (`Form[i].YearGroup`, `^arr[i]`) — never
hand-roll indexing or element loads. Array parameters: `bindArrayParameter` rebuilds
callee-side metadata from the flattened `ptr + 2R bounds` ABI (BYVAL = malloc+memcpy
whole copy), `emitArrayArgument` validates and pushes the caller side.
`OUTPUT arr` (and partially-indexed accesses) is intercepted by
`ArrayHandler::tryEmitArrayOutput` — the first branch of OUTPUT dispatch, emitting
nested print loops (element kinds that are not outputtable, i.e. records/pointers,
are a compile error there) — so any rework of OUTPUT must preserve that interception.

**Functions** (`FunctionGen`): plain LLVM functions, external linkage, exact pseudocode
name. Params are `ParamDecl{name, typeString, mode, array rank/bounds}`. BYVAL args are
copied into entry-block allocas (whole structs for records); BYREF args bind the
incoming pointer directly as the variable's storage; array params are handed to the
binder callback CodeGen injects (`setArrayParamBinder`) so FunctionGen never sees
ArrayHandler. Missing RETURNS defaults the return type to INTEGER; procedures use
"VOID". `emitPrototype` records each function's pseudocode signature in a `FuncSig`
map (`FunctionGen::getSignature`); `CodeGen::marshalCallArgs` dispatches call arguments
on those *declared* `ParamSig`s, and `getExprTypeInfo` reads return types from the same
map. Top-level prototypes are pre-registered at the start of `compile()` (after the
TYPE pre-pass), so calls may precede definitions; calling a name with no signature is a
compile error (no extern is fabricated). Body emission is inverted: CodeGen injects a
`StmtEmitter` callback, and must restore its own insert point (and the exchanged
ArrayTable) afterwards.

**User types** (`TypeSystem` + `Designator.cc`): TYPE declarations are top-level only
and registered by `CodeGen::runTypePrePass` in two passes — `declareUserType` reserves
every name (records become opaque named structs, so pointer pointees may
forward-reference: `TYPE NodePtr = ^Node` before `TYPE Node`), then
`defineEnum/defineRecord/definePointer` fill payloads in source order (a by-value
record field needs its record complete; inclusion via pointer is always fine). Enum
constants live in a TypeSystem-owned table, resolved in `emitExpr`'s
`VariableExprAST` branch after `Symbols` misses. The lvalue layer
(`emitLValue`/`loadFromLValue`) resolves designator chains to `{address, type name}`;
`emitCoercedExpr` is the single name-level admission gate; `emitUserKindBinaryOp`
intercepts binary operators ahead of ArithmeticHandler (which, like StringHandler and
FileHandler, is deliberately ignorant of user kinds). The enum range check after
`E ± INTEGER` is the only site that can produce an invalid ordinal — every other
producer (constants, same-name copies, zero-init, memset-0 arrays) is valid by
construction, which is why 0-based ordinals are load-bearing.

**Runtime checks** (`RuntimeCheck`): only array bounds checking and the file checks are
actually wired up. `emitDivZeroCheck` exists but has **zero call sites** — division by
zero (for `/`, DIV and MOD) is currently unchecked; the TODO sits in
`ArithmeticHandler.cc`. The public `emitErrorAndExit(Cond, Msg, ArrayRef<Value*>)`
overload exists for failure sites whose printf arguments (line, filename) are runtime
values — FileHandler's helpers use it; the private compile-time-line form delegates to
it.

**Files** (`FileHandler`): text-file runtime for §9.1. StringHandler-shaped leaf —
holds Context/Builder/Module/RuntimeCheck refs only, never sees CodeGen, AST nodes or
the symbol table; `CodeGen::emitStmt`'s four file branches (and the EOF builtin
intercept) evaluate/coerce operands and hand over `llvm::Value*`s plus the statement
line. On first file use, `ensureRuntime()` synthesizes the whole runtime once — the
only module-level helper functions in the project: internal-linkage `__cps_file_find/
open/read/write/close/eof` plus a 16-slot open-file table (`__cps_file_names/handles/
modes` parallel globals; mode byte 0 = free). Files are keyed by filename *content*
(strcmp scan) because STRING `=` is pointer identity; the name pointer is stored as-is
(strings are immutable and never freed). EOF is an fgetc/ungetc peek; READFILE grows a
malloc/realloc buffer per line and strips one trailing `\r`; fopen modes are text
`"r"/"w"/"a"` (Windows CRT translates CRLF). File-free programs get byte-identical IR —
nothing is declared or emitted eagerly. Helper bodies are built with the *shared*
Builder (RuntimeCheck emits through the same one), saving/restoring the insert block.
