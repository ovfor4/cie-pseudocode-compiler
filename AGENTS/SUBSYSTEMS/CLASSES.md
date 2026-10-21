# Classes: declarations and methods

Part of the [subsystem notes](../SUBSYSTEMS.md). Builds on [user types](USER_TYPES.md);
construction and destruction are in [class lifetime](CLASS_LIFETIME.md).

## Declaration and access

- `ClassTypeDeclAST` inherits `RecordTypeDeclAST`: CLASS declarations use the same
  two-pass registration, `TypeKind::Record`, `%record.NAME` layout and value semantics.
- Shared body/field parsing lives in `ParserType.cc`.
- TYPE fields always have `IsPublic = true`; CLASS fields default to public or accept an
  explicit PUBLIC/PRIVATE keyword and carry that flag through `defineRecord` into
  `RecordFieldInfo`.
- Both the static designator type walk and `emitLValue` check private field access
  against the current method's owning class, including address-of, array elements and
  pointer dereferences. Other instances of the same class are accessible from its
  methods.
- `TypeInfo::IsClass` distinguishes classes from records without changing their shared
  LLVM representation.

## Methods

- Class ASTs also own their `FunctionDefAST` methods. PrototypeAST retains the owner and
  access level; `getLinkName()` supplies the source key `Class.Method`.
- FunctionGen appends `(canonical parameter types)` only for OVERLOAD, leaving ordinary
  symbols unchanged.
- CodeGen registers methods separately in `ClassMethods` overload vectors and
  pre-registers their prototypes after the type pre-pass.
- `resolveMethod` uses `resolveCall`; access is checked on the selected overload.
  Constructor/default-construction paths select NEW through this same resolver.
- `MemberCallExprAST` keeps an addressable receiver separate from explicit arguments;
  `MemberCallStmtAST` discards its result.
- `emitMemberCall` checks access and prepends the receiver address to arguments
  marshalled by the existing `marshalCallArgs`. The hidden receiver is not part of
  FuncSig's explicit parameter list.
- FunctionGen binds field aliases and `this` to the incoming receiver storage before
  binding explicit parameters.
- CodeGen saves/restores `CurrentClass` around each function body; unqualified calls
  still resolve globally.
- Local declarations and parameters may shadow fields with a source-line warning;
  `this.Field` remains explicit. `this` cannot be redeclared within methods.
