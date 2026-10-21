# Functions and operator overloads

Part of the [subsystem notes](../SUBSYSTEMS.md). See also [EXTERN "C"](EXTERN.md) and
[templates](TEMPLATES.md).

## Functions (`FunctionGen`)

Plain LLVM functions, external linkage, exact pseudocode name unless OVERLOAD appends a
canonical parameter list (MAIN uses internal __cps_main).

### Parameters and bodies

- Params are `ParamDecl{name, typeString, mode, array rank/bounds}`.
- BYVAL args are copied into entry-block allocas (whole structs for records); BYREF args
  bind the incoming pointer directly as the variable's storage.
- Array params are handed to the binder callback CodeGen injects
  (`setArrayParamBinder`) so FunctionGen never sees ArrayHandler.
- Missing RETURNS defaults the return type to INTEGER; procedures use "VOID".
- Body emission is inverted: CodeGen injects a `StmtEmitter` callback, and must restore
  its own insert point (and the exchanged ArrayTable) afterwards.
- Functions inherit visible GLOBAL storage and previously declared own top-level
  CONSTANT values, plus imported constants from the start; local constants use the same
  Symbols table with lexical binding restoration.

### Signatures and call resolution

- `emitPrototype` records each function's pseudocode signature in a `FuncSig`
  overload-set map (`FunctionGen::getOverloads`, filtered by unit visibility).
- `resolveCall` selects exact static parameter types/ranks for OVERLOAD, while a single
  ordinary definition keeps implicit conversions.
- Parameter keys exclude return types, modes and bounds (they never distinguish
  overloads); equivalent C aliases use
  CIdentity. Every same-name definition must be OVERLOAD; duplicate keys fail even
  across invisible units.
- FunctionGen tracks prototype registration separately from source names and indexes
  signatures by LLVM link name for RETURN (`getSignatureByLink`).
- `CodeGen::marshalCallArgs` consumes the selected FuncSig, and `getExprTypeInfo`
  resolves the same calls without emitting IR.
- Prototype pre-registration (calls may precede definitions): see
  [CONVENTIONS/CALLS.md](../CONVENTIONS/CALLS.md).

## Operator overloads (`OperatorGen.cc`)

- PrototypeAST retains the operator token; FunctionGen validates operand count and
  requires a record/class/enum operand.
- Global signatures and left-class methods share exact-type selection, used by
  ValueTypeGen and ExpressionGen before builtin operations. Missing candidates leave
  builtin rules intact; ties are errors. Member access is checked after selection.
- The raw-expression argument-marshalling overload borrows AST pointers for operator
  calls; ordinary calls adapt their unique_ptr lists into it.
- Member receivers use lvalue storage or a materialized, Alive-tracked temporary.
- CASE calls the value entry point with its already captured selector and requires
  BOOLEAN results.
