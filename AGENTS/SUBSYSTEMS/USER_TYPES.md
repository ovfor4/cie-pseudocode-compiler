# User types

Part of the [subsystem notes](../SUBSYSTEMS.md). Classes build on these; see
[classes](CLASSES.md) and [class lifetime](CLASS_LIFETIME.md).

**User types** (`TypeSystem` + `Designator.cc` + `TypeCheckGen.cc`):
`Designator.cc` owns the type pre-pass and lvalue access; `TypeCheckGen.cc` owns type
admission and user-kind binary operators.

## Registration

TYPE declarations are top-level only and registered by `CodeGen::runTypePrePass` in two
passes:

1. `declareUserType` reserves every name (records become opaque named structs, so
   pointer pointees may forward-reference: `TYPE NodePtr = ^Node` before `TYPE Node`).
2. `defineEnum/defineRecord/definePointer` fill payloads in source order (a by-value
   record field needs its record complete; inclusion via pointer is always fine).

Enum constants live in a TypeSystem-owned table, resolved in `emitExpr`'s
`VariableExprAST` branch after `Symbols` misses.

## Lvalues, admission and operators

- The lvalue layer (`emitLValue`/`loadFromLValue`) resolves designator chains to
  `{address, type name}`.
- `emitCoercedExpr` is the single name-level admission gate.
- `emitUserKindBinaryOp` intercepts built-in binary operators ahead of ArithmeticHandler
  (which, like FileHandler, is deliberately ignorant of user kinds).
- The enum range check after `E ± INTEGER` is the only site that can produce an invalid
  ordinal — every other producer (constants, same-name copies, zero-init, memset-0
  arrays) is valid by construction, which is why 0-based ordinals are load-bearing.
