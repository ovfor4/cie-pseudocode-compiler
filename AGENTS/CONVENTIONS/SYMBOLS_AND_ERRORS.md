# Conventions: symbols, dispatch and diagnostics

Part of [Load-bearing conventions](../CONVENTIONS.md).

**One symbol namespace, one table.** `Symbols` (name → `SymbolInfo{Storage, TypeName,
IsArray, ConstantValue, ScopeID, IsReference, IsGlobal, ReadOnly}`) in CodeGen is the
only symbol table.

- Use `symbolAddress` for an object address: class variables and BYVAL class
  parameters store their current object address in a reference slot (`IsReference`).
- Use SymbolInfo::isConstant() for constant checks: literals, including raw
  C_STRING text constants, use ConstantValue and have no writable storage.
- Read values through emitExpr. Writable/address-taking paths reject constants
  through requireWritable; internal STRING copy/assignment source bindings may read
  their storage.
- Register variables through `registerSymbol()` (parameters are also bound directly
  by FunctionGen). `prepareSymbolDeclaration` diagnoses same-scope conflicts
  involving constants before declaration emission.
- Unique scope IDs distinguish sibling blocks; `emitScopedBlock` restores only
  bindings introduced by constants or hiding constants, preserving other variables'
  existing function-wide visibility.
- FunctionGen saves/restores Symbols and inherits visible globals plus previously
  visible top-level constants (ScopeID 0). Locals may shadow globals. CodeGen
  isolates constant scope bookkeeping and array metadata around each function body.
  Arrays also restore metadata at control-flow block exits.

**Designators degrade.** The parser turns `x` / `arr[i]` into the legacy
`VariableExprAST`/`ArrayAccessExprAST` (and their assign statements) and only builds
`DesignatorExprAST`/`DesignatorAssignStmtAST` for chains involving `.`, `^` or
multiple accessors — existing programs keep byte-identical ASTs. `CodeGen::emitLValue`
resolves any of these to `{address, type name}`, emitting index and null-deref checks
on the way; `loadFromLValue` owns the rvalue read of the resolved value.

**Dispatch is by `dynamic_cast` chain, not virtual methods.** `CodeGen::emitStmtImpl`
in `StatementGen.cc` and `emitExpr` in `ExpressionGen.cc` walk linear if-chains of
casts. **A statement type missing from the chain is silently ignored** — no error.
Adding an AST node always means touching these chains. Emit statements through
`emitStmt`, whose wrapper cleans up statement temporaries after dispatch.

**Compiler error style: print an `error:` line, set the component's error flag, and
keep compiling.** No exceptions, no exit mid-compile.

- `Lexer`/`Parser`/`CodeGen` each expose `hadError()`; AST expressions/statements
  share source-line metadata. Parser entry points record token locations. CodeGen
  saves/restores its diagnostic line across recursive emission and coercion.
- Diagnostics use the gcc layout `<path>:<line>: error: <message>` (`<path>:` when
  the line is unknown). Lexer/parser/driver call `cps::sourceError(Line, fmt, ...)`
  (`sourceWarning` for warnings) from `Diagnostics.h` with the bare message: no
  `error:` label, location or newline. `reportError` routes through the same helper
  with `DiagnosticLine`. `DiagnosticSource` holds the unit path as given on the
  command line.
- `CodeGen::reportError(fmt, ...)` is the helper for codegen-side diagnostics
  (`ArrayHandler` reaches it through its `CodeGen &` parameter,
  `FunctionGen`/`ArithmeticHandler` hold a `bool &HadError` reference).
- The driver exits non-zero when any flag is set, printing `compilation terminated.`
  once. Errors that prevent compilation (missing input, file I/O or invalid IR) use
  `fatal error:`. Execution-stage errors do not print the compilation termination
  line.
- The parser drops a failed statement and resyncs at the next token that can start a
  statement or end a block (`syncToStatementStart`), then continues; the failure is
  always diagnosed and reflected in the exit code.
