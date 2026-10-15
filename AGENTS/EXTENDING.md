# Extension recipes

Step-by-step patterns for adding features. Read `CONVENTIONS.md` first — every recipe
below assumes those invariants.

**New statement:** token in `Lexer.h` enum + keyword match in `Lexer.cc`'s if-chain →
node class in `AST.h` or a feature header (`FunctionAST.h`/`FileAST.h` precedent;
unique_ptr children, move ctor, raw-ptr getters) → `ParseXxx` in
`Parser.cc` + branch in `ParseStatementImpl` (block statements parse their body with
`ParseBlock({terminator tokens})`; comma-separated expression lists use
`ParseExprList(close, allowEmpty)`; add the new statement's leading keyword to
`syncToStatementStart`) → `dynamic_cast` branch in `CodeGen::emitStmt`. Guard every
fall-through branch in control-flow codegen with
`if (!Builder->GetInsertBlock()->hasTerminator())` — a RETURN in the body may have
already terminated the block.

**New builtin function:** add a `{Name, Arity, ReturnTypeName}` row to the static
`Builtins` table at the top of `CodeGen.cc` (single source of truth — arity checking in
`emitExpr`, the return type in `getExprTypeInfo`, and the reject-user-redefinition check
all read it), then implement IR emission as a method on the fitting handler
(String/StringConversion) or inline, and add the name intercept in `emitExpr`'s
CallExprAST section — null-check every emitted argument before use, like the existing
branches do. Builtins are plain identifiers — nothing is needed in lexer/parser.

**New binary operator:** token (if multi-char) → one line in `BinopPrecedence`
(Parser ctor; levels: OR=3, AND=5, comparisons=10, `+ - &`=20, `* / DIV MOD`=40;
precedence must be > 0) → branch in `ArithmeticHandler::emitBinaryOp`, which dispatches
on raw LLVM operand types. Exceptions in `CodeGen::emitExpr`: `&` coerces operands
to STRING then calls `StringHandler::emitConcat`; STRING comparisons require two
STRING operands by pseudocode type then call `StringHandler::emitCompare`.

**New builtin type:** register in `TypeSystem::registerBuiltins` (sizes are derived —
never hand-written) + `TypeKind` case in the **exhaustive, default-less switches**:
`getZeroValue`, `coerceValueToType`, `emitOutputValue`, `TypeInfo::isOutputtable`,
the INPUT branch in CodeGen + lexer keyword + `ParseTypeName` case. The compiler
warns on a missing enumerator — that is the anti-silent-fall-through mechanism;
never add a `default`.

**New user-defined type KIND (like enum/record/pointer):** payload struct + checked
accessor in `TypeInfo`, `declareUserType`/`defineXxx` pair in TypeSystem (two-pass:
names then definitions), decl AST node in `TypeAST.h`, `ParseTypeDecl` branch,
pre-pass branch in `CodeGen::runTypePrePass`, admissibility rules in
`emitCoercedExpr` + `emitUserKindBinaryOp`, and a decision for every hole:
OUTPUT/INPUT/WRITEFILE/file-name operands, FOR variables, builtin args,
`getExprTypeInfo` classification. Everything not explicitly admitted must be a
compile error.

**Native C types:** add representation/format metadata in TypeSystem's native
registration, using compiler-derived facts rather than fixed widths. Preserve
canonical CIdentity and alias compatibility. Update CTypeGen conversions,
promotions and I/O together; never let native C operands fall through to the
legacy ArithmeticHandler or representation-only coercions. C_TYPE parses a
string import, not an ordinary function call. Casts are CastExprAST values;
their type must be recognized by both emission and getExprTypeInfo.

**New designator access form:** `AccessKind` case in `TypeAST.h` +
`ParseDesignatorSuffix` + the walk in `CodeGen::emitLValue` **and** the static
walk in `getExprTypeInfo`'s designator branch (they must agree) — plus the
degrade rule in `ParseIdentifierExpr`/`ParseAssignStmt` if the new form can
collapse to a legacy node.

**New runtime check:** add `emitXxxCheck` in `RuntimeCheck` (build an i1
fail-condition and call `emitCheck` with a diagnostic format and arguments — it
splits the block and leaves the builder at the continuation), then call it from
the emit site. Failure prints the source line and error reason to stdout before
throwing an anonymous exception; generated main supplies the uncaught message
and status 1. Precedents: `emitIndexCheck`, `emitEnumRangeCheck`, `emitNullDerefCheck`,
`emitArrayArgBoundsCheck` and FileHandler's direct `emitCheck` calls.

**New potentially throwing call:** route it through `ExceptionHandler::emitCall`
so it invokes the current handler or catch cleanup when needed. Give generated
functions unwind tables via `prepareFunction`. Exception ABI dependencies must
also be registered in EmbeddedRunner for static-runtime JIT symbol lookup.

**New libc dependency:** declare via `getOrInsertFunction` in the owning component's
`setupExternalFunctions()` (re-declaring the same symbol in two handlers is fine — it
dedupes by name).
