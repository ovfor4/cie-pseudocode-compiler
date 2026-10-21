# Conventions: type system

Part of [Load-bearing conventions](../CONVENTIONS.md).

**Type names travel as `std::string`** (uppercase builtins such as `"INTEGER"` and
`"REAL"`; user-defined names retain their spelling) — never an enum. `TypeSystem`
resolves the string to `TypeInfo{LLVMType, Kind, per-kind payload}`. The parser's
`ParseTypeName` accepts *any* identifier as a type name except `VOID` (rejected
outside a RETURNS clause via its `AllowVoid` flag); other unknown types only fail
(softly) in CodeGen.

**Fixed IR representation per builtin type.** The table lives in
[ARCH/OVERVIEW.md](../ARCH/OVERVIEW.md). Some arithmetic code still infers types
backwards from LLVM widths; preserve the builtin scalar ABI.

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
identity is case-insensitive name equality (`P1 = ^INTEGER` and `P2 = ^INTEGER` are
incompatible; same-layout records too). Their LLVM representations are
indistinguishable from builtins, so values of these kinds must NEVER reach
`ArithmeticHandler` or `coerceValueToType` without a prior name-level check —
`CodeGen::emitCoercedExpr` (assignments, BYVAL args, RETURN, IF/WHILE/REPEAT/FOR
operands, file operands, builtin args) and the typed `CodeGen::emitBinaryOp`
dispatcher in `ExpressionGen.cc` guard these paths. The dispatcher resolves overloads
and handles C/user kinds before builtin arithmetic. `coerceValueToType` hard-errors
("Internal: ...") if a user-kind target reaches it.

**Enum value names are a flat, globally reserved namespace** owned by `TypeSystem`
(not `Symbols`, so they survive FunctionGen's save/clear/restore). Expression
resolution order is `Symbols` first, then enum constants; collisions with
DECLARE/array/parameter/function names are compile errors at declaration time, so
the order can never shadow. TYPE declarations are top-level only and registered in
two passes; see [SUBSYSTEMS/USER_TYPES.md](../SUBSYSTEMS/USER_TYPES.md#registration).
