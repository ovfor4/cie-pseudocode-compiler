# Extension recipes

Step-by-step patterns for adding features. Read `CONVENTIONS.md` first — every recipe
below assumes those invariants.

**New statement:** token in `Lexer.h` enum + keyword match in `Lexer.cc`'s if-chain →
node class in `AST.h` (unique_ptr children, move ctor, raw-ptr getters) → `ParseXxx` in
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

**New type:** register in `TypeSystem::registerBuiltins` (correct `ElementSize` —
arrays malloc `count * ElementSize`) + `TypeKind` case + `getZeroValue` +
`coerceValueToType` + `emitOutputValue` + INPUT branch in CodeGen + lexer keyword +
`ParseTypeName` case.

**New runtime check:** message global + `emitXxxCheck` in `RuntimeCheck` (build an i1
fail-condition, call `emitErrorAndExit` — it splits the block and leaves the builder at
the continuation), then call it from the emit site with the AST node's `getLine()`.
Precedent: `emitIndexCheck`, called per-dimension from ArrayHandler. Note that failure
messages print to **stdout** (`[Fatal] line %d: ...`) and `exit(1)`.

**New libc dependency:** declare via `getOrInsertFunction` in the owning component's
`setupExternalFunctions()` (re-declaring the same symbol in two handlers is fine — it
dedupes by name).
