# Subsystem notes

Deep-dives on the CodeGen helper subsystems. (Invariants: `CONVENTIONS.md`;
known bugs: `PITFALLS.md`.)

**Arrays** (`ArrayHandler`): allocation/indexing live in `ArrayHandler.cc`,
parameter binding/arguments in `ArrayParameters.cc`, and output in `ArrayOutput.cc`.
A DECLAREd array is one malloc'd flat buffer (memset to
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
`ArrayHandler::tryEmitArrayOutput` — the first branch for each expression in an OUTPUT
list, emitting nested print loops (element kinds that are not outputtable, i.e. records/pointers,
are a compile error there). Arrays keep their per-element newlines and dispatch
continues with the next expression; a final array requires no extra newline.

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

`ClassTypeDeclAST` inherits `RecordTypeDeclAST`: CLASS declarations use the same
two-pass registration, `TypeKind::Record`, `%record.NAME` layout and value semantics.
Shared body/field parsing lives in `ParserType.cc`. TYPE fields always have
`IsPublic = true`; CLASS fields require an explicit PUBLIC/PRIVATE keyword and
carry that flag through `defineRecord` into `RecordFieldInfo`. Both the static
designator type walk and `emitLValue` reject private field access, including
address-of, array elements and pointer dereferences. There is no class method
context or language-level inheritance yet.

**Runtime checks** (`RuntimeCheck`): array bounds, file misuse, enum ranges, null
dereferences and array-parameter bounds contracts are checked at their emit sites.
`ArithmeticHandler` calls `emitDivZeroCheck` before `/`, DIV and MOD; the check accepts
integer and floating-point divisors, including both floating-point signed zeros.
`emitCheck(Cond, Msg, Args)` splits the block, prints the source line and error
reason to stdout, and calls `ExceptionHandler::emitThrow` on failure. File helper
checks receive the source line as a runtime argument.

**Exceptions** (`ExceptionHandler`): holds Context/Builder/Module refs and a map of
per-function handler stacks and active catch depths. TRY pushes a catch target and
an entry-block alloca for its exception pointer. A potentially throwing call with
a handler or active catch emits an `invoke` and a landing pad that finishes catches being left and transfers
to the nearest handler, or resumes unwinding when only cleanup is needed. CATCH
uses `__cxa_begin_catch` and removes its own TRY target before emitting its body;
normal exit and RETURN call `__cxa_end_catch`. Bare THROW always creates a new
anonymous integer exception, using `_ZTIi` internally without exposing a value.
Generated main owns the final catch target, prints the generic uncaught message,
and returns 1. Native binaries require the C++ runtime; EmbeddedRunner explicitly
registers its statically linked ABI symbols, while MCJIT manages unwind frames.
This does not change heap-string/array lifetimes or automatically close files.

**Files** (`FileHandler`): text-file runtime for §9.1.
`FileHandler.cc` owns setup, lookup/open and emission entry points;
`FileRuntimeIO.cc` builds the read/write/close/EOF helpers. StringHandler-shaped leaf —
holds Context/Builder/Module/RuntimeCheck/ExceptionHandler refs only, never sees
CodeGen, AST nodes or the symbol table; `CodeGen::emitStmt`'s four file branches (and the EOF builtin
intercept) evaluate/coerce operands and hand over `llvm::Value*`s plus the statement
line. Calls into throwing helpers go through ExceptionHandler. On first file use,
`ensureRuntime()` synthesizes the whole runtime once — the
only module-level helper functions in the project: internal-linkage `__cps_file_find/
open/read/write/close/eof` plus a 16-slot open-file table (`__cps_file_names/handles/
modes` parallel globals; mode byte 0 = free). Files are keyed by filename *content*
(strcmp scan), consistent with STRING comparison; the name pointer is stored as-is
(strings are immutable and never freed). EOF is an fgetc/ungetc peek; READFILE grows a
malloc/realloc buffer per line and strips one trailing `\r`; fopen modes are text
`"r"/"w"/"a"` (Windows CRT translates CRLF). File-free programs get byte-identical IR —
nothing is declared or emitted eagerly. Helper bodies are built with the *shared*
Builder (RuntimeCheck emits through the same one), saving/restoring the insert block.
