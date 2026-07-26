# Subsystem notes

Deep-dives on the CodeGen helper subsystems. (Invariants: `CONVENTIONS.md`;
known bugs: `PITFALLS.md`.)

**Arrays** (`ArrayHandler`): a DECLAREd array is one malloc'd flat buffer (memset to
zero), row-major, any rank, any bounds. Bounds and multipliers are *compile-time SSA
values* cached in `ArrayMetadata` (global `ArrayTable` map, keyed by name). Every access
goes through `emitCheckedIndices` (validates the index count for the caller's semantics,
then per-dimension coerce + `emitIndexCheck`) → `computeFlatIndex` → `loadElement`
(load + the STRING null-guard `select` to the empty string); never hand-roll indexing
or element loads.
`OUTPUT arr` (and partially-indexed accesses) is intercepted by
`ArrayHandler::tryEmitArrayOutput` — the first branch of OUTPUT dispatch, emitting
nested print loops — so any rework of OUTPUT must preserve that interception.

**Functions** (`FunctionGen`): plain LLVM functions, external linkage, exact pseudocode
name. Params are `tuple<name, typeString, isByRef>`. BYVAL args are copied into
entry-block allocas; BYREF args bind the incoming pointer directly as the variable's
storage. Missing RETURNS defaults the return type to INTEGER; procedures use "VOID".
`emitPrototype` records each function's pseudocode signature in a `FuncSig` map
(`FunctionGen::getSignature`); `CodeGen::marshalCallArgs` dispatches call arguments on
those *declared* param types and BYREF flags (BYVAL args are coerced to the declared
type), and `getExprTypeInfo` reads return types from the same map. Top-level prototypes
are pre-registered at the start of `compile()`, so calls may precede definitions;
calling a name with no signature is a compile error (no extern is fabricated). Body
emission is inverted: CodeGen injects a `StmtEmitter` callback, and must restore its
own insert point afterwards.

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
