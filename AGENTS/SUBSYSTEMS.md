# Subsystem notes

Deep-dives on the three CodeGen helper subsystems. (Invariants: `CONVENTIONS.md`;
known bugs: `PITFALLS.md`.)

**Arrays** (`ArrayHandler`): a DECLAREd array is one malloc'd flat buffer (memset to
zero), row-major, any rank, any bounds. Bounds and multipliers are *compile-time SSA
values* cached in `ArrayMetadata` (global `ArrayTable` map, keyed by name). Every access
goes through `getMetadata` → per-dimension `emitIndexCheck` → `computeFlatIndex` →
`getElementPointer`; never hand-roll indexing. STRING elements read as null until
written; every read path must replicate the null-guard `select` to the empty string.
`OUTPUT arr` (and partially-indexed accesses) is intercepted by
`ArrayHandler::tryEmitArrayOutput` — the first branch of OUTPUT dispatch, emitting
nested print loops — so any rework of OUTPUT must preserve that interception.

**Functions** (`FunctionGen`): plain LLVM functions, external linkage, exact pseudocode
name. Params are `tuple<name, typeString, isByRef>`. BYVAL args are copied into
entry-block allocas; BYREF args bind the incoming pointer directly as the variable's
storage. Missing RETURNS defaults the return type to INTEGER; procedures use "VOID".
**Definitions must precede first call**: calling a not-yet-defined function fabricates
an extern declaration (i64 return, param types guessed from the arguments), and when the
real definition arrives, `emitFunctionDef` reuses that declaration by name without
checking the signature — the body attaches to the wrong prototype and the module is
invalid. Body emission is inverted: CodeGen injects a `StmtEmitter` callback, and must
restore its own insert point afterwards.

**Runtime checks** (`RuntimeCheck`): only array bounds checking is actually wired up.
`emitDivZeroCheck` exists but has **zero call sites** — division by zero (for `/`, DIV
and MOD) is currently unchecked; the TODO sits in `ArithmeticHandler.cc`.
