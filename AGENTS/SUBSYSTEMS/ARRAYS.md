# Arrays

Part of the [subsystem notes](../SUBSYSTEMS.md). Invariants:
[`CONVENTIONS.md`](../CONVENTIONS.md); known bugs: [`PITFALLS.md`](../PITFALLS.md).

**Arrays** (`ArrayHandler`): allocation/indexing live in `ArrayHandler.cc`,
parameter binding/arguments in `ArrayParameters.cc`, and output in `ArrayOutput.cc`.

## Storage and metadata

- A DECLAREd array is one malloc'd flat buffer (memset to zero), row-major, any rank,
  any bounds.
- Every dimension needs `upper >= lower`: constant bounds are a compile error, other
  bounds a runtime error (`emitCheckedByteSize`). The byte size is computed with
  `llvm.{ssub,umul}.with.overflow`; overflow or a NULL malloc (also the BYVAL copy)
  reports `Array X is too large`. Arrays therefore always have at least one element.
- Bounds and multipliers are *SSA values* cached in `ArrayMetadata`.
- `ArrayTable` is scoped per function (`exchangeTable`, driven from CodeGen's
  `FunctionDefAST` branch, mirroring the Symbols discipline).
- Element sizes are `sizeof` constants (`ElementSizeC`), never host-side numbers.

## Indexing pipeline

Every access goes through `emitCheckedIndices` (validates the index count for the
caller's semantics, then per-dimension coerce + `emitIndexCheck`) →
`computeFlatIndex` → `loadElement` (load + the typed value load).

The public `emitElementAddress` exposes the same checked pipeline as an address for the
lvalue layer (`Form[i].YearGroup`, `^arr[i]`) — never hand-roll indexing or element
loads.

## Array parameters

- `bindArrayParameter` rebuilds callee-side metadata from the flattened
  `ptr + 2R bounds` ABI (BYVAL = allocated buffer copy with independent STRING-bearing
  elements).
- `emitArrayArgument` validates and pushes the caller side.

## Array output

`OUTPUT arr` (and partially-indexed accesses) is intercepted by
`ArrayHandler::tryEmitArrayOutput` — the first branch for each expression in an OUTPUT
list, emitting nested print loops (element kinds that are not outputtable, i.e.
records/pointers, are a compile error there). Arrays keep their per-element newlines and
dispatch continues with the next expression; a final array requires no extra newline.
