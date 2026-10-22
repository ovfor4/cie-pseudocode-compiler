# EXTERN "C" and INCLUDE_OBJ

Part of the [subsystem notes](../SUBSYSTEMS.md). Related: [functions](FUNCTIONS.md).

**EXTERN "C"** (`ExternDeclAST`, `IncludeObjStmtAST`)

## Declarations and calls

- The parser builds a body-less `PrototypeAST` (IsExternal, IsVariadic, unnamed `argN`
  params).
- `FunctionGen::emitExternPrototype` admits types with `TypeSystem::isExternType`
  (after `pointerBase` follows pointer types: a non-void C type or a C struct;
  `^INTEGER` and cycles such as `TYPE P = ^P` fail), records a `FuncSig` with
  `IsExternal` and the clang-arranged `FuncSig::ExternABI` (see below), and
  `getOrInsertFunction`s the exact symbol.
- Identical redeclarations append a FuncSig per declaring unit (visibility unchanged)
  and `resolveCall` takes the first visible one; mismatched signatures are errors.
- Calls go through `FunctionGen::emitCall(const FuncSig &, ...)` with that type, so a
  runtime declaration of the same libc symbol with another type is harmless.
- `CodeGen::emitVariadicArgument` applies C default promotions to C scalars and
  passes C-struct records and pointers to them unchanged (C11 §6.5.2.2p6–7,
  §7.16.1.1p2). `^x` of a C value or C struct passes as `void*`, like C's `&x`; a
  pseudocode-typed `^x` keeps "must be a C value; cast it to a C type". Each variadic
  call is arranged separately from the fixed-parameter count and the promoted types.

## C structs and the C ABI

- `TypeSystem::isCStruct`: a TYPE record (not a CLASS) whose fields, after
  `pointerBase`, are non-void C types or C structs. A seen list stops recursion, so
  self-referential lists terminate. Any pseudocode field type (INTEGER, REAL, CHAR,
  BOOLEAN, STRING, enum, CLASS) fails: `<What> uses record <Rec>, which is not a C
  struct: field <Field> of <Owner> has type <Type>`, where `<What>` is `EXTERN
  parameter N of F`, `The return type of EXTERN_FUNCTION F` or `Variadic argument N
  to F`. Other unsupported types keep `EXTERN parameter N of F must be a non-void C
  type (got X)`.
- Records already have C layout (`TypeSystem::defineRecord`: non-packed named
  struct), so no conversion is needed to pass a pointer.
- Every EXTERN signature, scalar-only ones included, is arranged by clang through the
  FunctionGen member `ClangABI ABI`: `ClangABI::arrange` (`lib/CodeGen/CABI/ClangABI.cc`)
  calls `clang::CodeGen::arrangeFreeFunctionCall` and translates each `ABIArgInfo`
  into a `CABISignature` of `CABIArg`s (`include/cps/CABI.h`; Direct, Extend,
  Indirect, Ignore; any other kind is an internal error).
  - The lazy `ClangABI::State`, created on the first EXTERN, owns the clang
    CodeGenerator. It targets `llvm::sys::getProcessTriple()` with options from
    `clang::createInvocation`, so darwinpcs and char signedness match the C compiler.
  - Scalars map by name, all pointers to `void*`, and each record to a cached
    `RecordDecl`. A size mismatch is an internal error: `Internal: cannot arrange
    EXTERN F: record R has N bytes in cpsc but M in C` (`cannot arrange the call to
    F` for a variadic call).
  - Keep clang code in this file: `clang::CodeGen` clashes with `cps::CodeGen`, clang
    classes cannot be subclassed across the RTTI mismatch, and the CodeGenerator must
    be destroyed before the ASTContext.
- `lib/CodeGen/CABI/Lowering.cc` applies the description without architecture
  branches: `getCABIFunctionType`, `getCABIAttributes` (sret/byval/align/signext/
  zeroext), `lowerCABIArguments` (coercion through a temporary
  `alloca {RecordTy, CoerceTy}`, flattening, copies for Indirect) and
  `lowerCABIResult` (sret, or a coerced return stored back into a record).
- The result matches the platform C compiler on Linux x86-64, Windows x86-64 (MinGW)
  and macOS arm64. The latter two pass large records as a pointer to a copy
  (non-byval Indirect); only nightly CI tests that path. MinGW `long double` should
  now pass by reference, as in C, but is untested.

## Driver and object files

- The driver (`main.cc`) resolves INCLUDE_OBJ paths, reads defined global symbols with
  `llvm::object` (`collectObjectSymbols`; `"__libc"` uses `collectLibcSymbols` over the
  embedded runtime's libc/libm inputs, copied to aligned buffers), rejects missing
  symbols, and passes objects to `compileNative`, `runEmbedded` (`addObjectFile`) or lli
  (`-extra-object=`).
- On Windows, `runEmbedded` adds a msvcrt.dll search generator ahead of process
  symbols.
- Nightly CI runs extern-libc and the object-file fixtures in EMBEDDED mode on Linux,
  Windows and macOS; PATH execution depends on the external lli runtime.
