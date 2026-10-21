# Conventions: calls, parameters and builtins

Part of [Load-bearing conventions](../CONVENTIONS.md). Templates and operator
overloads, which build on call resolution, are in [TEMPLATES_AND_OPERATORS.md](TEMPLATES_AND_OPERATORS.md).

**Calls share argument marshalling.** `CodeGen::emitCall` handles global expression
calls, CALL statements and bare `name(args)` statements; `emitMemberCall` resolves
member access and adds a hidden receiver address. Both use `marshalCallArgs` and
FunctionGen for user calls; statement calls discard results. Explicit casts may
bridge C void* and nominal pseudocode pointer types.

**User calls dispatch on the declared signature.**

- `FunctionGen::emitPrototype` records every function's pseudocode signature
  (`FuncSig`: return type name + per-param `ParamSig` {type name, mode, array
  rank/bounds}).
- `CodeGen::marshalCallArgs` — the single call-marshalling path for both call-expr
  and call-stmt — requires BYREF arguments to be bare variables of *exactly* the
  declared type (their `symbolAddress` is passed), admits BYVAL arguments through
  `emitOwnedValue`, and dispatches whole-array arguments to
  `ArrayHandler::emitArrayArgument`.
- RETURN also uses `emitOwnedValue` against the declared return type. Class lvalues
  invoke a same-class BYREF OVERLOAD NEW when defined; STRING-bearing aggregates
  clone their owned fields. Other classes without a copy constructor copy bits; class
  rvalues transfer ownership. Other values use `emitCoercedExpr`.
- Top-level prototypes are pre-registered at the start of `compile()` (after the TYPE
  pre-pass) before statement emission, so call-before-definition is fine; a call with
  no registered signature is a compile error (no extern is fabricated). Method
  prototypes are pre-registered too.
- OVERLOAD signatures append canonical parameter types to the link name; ordinary
  names except MAIN get no mangling. Overload sets, visibility filtering and exact
  selection: [SUBSYSTEMS/FUNCTIONS.md](../SUBSYSTEMS/FUNCTIONS.md#signatures-and-call-resolution).
- In all cases, `emitPrototype` rejects names that collide with runtime/libc symbols
  (`isReservedRuntimeName`), builtins, or enum values. Signatures retain the first
  declaration spelling for LLVM lookup, independently of call-site casing.
- Methods use `Class.Method` names and a hidden leading receiver address; their
  FuncSig parameters contain only explicit source parameters.

**Parameter modes are sticky and BYREF is procedure-only (guide §8.3).** In
`(BYREF a : T, b : T)` the `b` is BYREF too; unannotated parameters default to BYVAL
(§8.1). A BYREF parameter in a FUNCTION is a parse error.

**Array parameters**: `ARRAY OF T` (rank 1, bounds travel with the argument) or
`ARRAY[l:u,...] OF T` (literal bounds, any rank, a contract checked at compile time
when the caller's bounds are constants, else at run time by throwing an exception).
The LLVM ABI flattens each rank-R array parameter into a data `ptr` plus
`kBoundArgsPerDim` (=2) i64 bounds per dimension; the callee rebuilds its
`ArrayMetadata` from those (`ArrayHandler::bindArrayParameter` via the binder
callback CodeGen injects into FunctionGen). BYVAL arrays copy their buffer at callee
entry and clone STRING-bearing elements; BYREF binds the caller's buffer.

**Builtins are plain identifiers backed by one table.** Builtins
(EOF/MALLOC/FREE/REALLOC) parse as ordinary calls — no lexer/parser support, and the
names are not reserved words. The inline constexpr `Builtins` table in the internal
`lib/CodeGen/Builtins.h` ({Name, Arity, ReturnTypeName}) is the single source of
truth: `emitCall` checks arity against it before dispatching to the per-builtin emit
branches (still an if-chain, ahead of the user-function fallback), and
`getExprTypeInfo` reads return types from it. EOF and the memory builtins consume the
call source line for runtime diagnostics; they touch mutable runtime state and can
throw.
