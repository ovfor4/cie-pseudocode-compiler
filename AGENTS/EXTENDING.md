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
`if (!Builder->GetInsertBlock()->getTerminator())` — a RETURN in the body may have
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
on raw LLVM operand types. Exception: `&` is intercepted upstream in `CodeGen::emitExpr`
(operands coerced to STRING, then `StringHandler::emitConcat`).

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

**New designator access form:** `AccessKind` case in `TypeAST.h` +
`ParseDesignatorSuffix` + the walk in `CodeGen::emitLValue` **and** the static
walk in `getExprTypeInfo`'s designator branch (they must agree) — plus the
degrade rule in `ParseIdentifierExpr`/`ParseAssignStmt` if the new form can
collapse to a legacy node.

**New runtime check:** message global + `emitXxxCheck` in `RuntimeCheck` (build an i1
fail-condition, call `emitErrorAndExit` — it splits the block and leaves the builder at
the continuation), then call it from the emit site with the AST node's `getLine()`.
Precedents: `emitIndexCheck` (per-dimension, from ArrayHandler), `emitEnumRangeCheck`
(after enum ± INTEGER — the only site that can manufacture an invalid ordinal),
`emitNullDerefCheck` (before every `^` deref), `emitArrayArgBoundsCheck` (bounded
array-parameter contract). Note that failure messages print to **stdout**
(`[Fatal] line %d: ...`) and `exit(1)`.

**New libc dependency:** declare via `getOrInsertFunction` in the owning component's
`setupExternalFunctions()` (re-declaring the same symbol in two handlers is fine — it
dedupes by name).
