# Subsystem notes

Deep-dives on the three CodeGen helper subsystems. (Invariants: `CONVENTIONS.md`;
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

**Runtime checks** (`RuntimeCheck`): only array bounds checking is actually wired up.
`emitDivZeroCheck` exists but has **zero call sites** — division by zero (for `/`, DIV
and MOD) is currently unchecked; the TODO sits in `ArithmeticHandler.cc`.
