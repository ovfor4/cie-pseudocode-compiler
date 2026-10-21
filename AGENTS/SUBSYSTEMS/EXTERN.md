# EXTERN "C" and INCLUDE_OBJ

Part of the [subsystem notes](../SUBSYSTEMS.md). Related: [functions](FUNCTIONS.md).

**EXTERN "C"** (`ExternDeclAST`, `IncludeObjStmtAST`)

## Declarations and calls

- The parser builds a body-less `PrototypeAST` (IsExternal, IsVariadic, unnamed `argN`
  params).
- `FunctionGen::emitExternPrototype` validates C types, records a `FuncSig` with
  `IsExternal`, the declared `FunctionType` and sext/zext attributes for sub-32-bit
  integers, and `getOrInsertFunction`s the exact symbol.
- Identical redeclarations append a FuncSig per declaring unit (visibility unchanged)
  and `resolveCall` takes the first visible one; mismatched signatures are errors.
- Calls go through `FunctionGen::emitCall(const FuncSig &, ...)` with that type, so a
  runtime declaration of the same libc symbol with another type is harmless.
- `CodeGen::emitVariadicArgument` applies C default promotions.

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
