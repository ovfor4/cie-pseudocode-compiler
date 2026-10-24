# Conventions: templates and operator overloads

Part of [Load-bearing conventions](../CONVENTIONS.md). Basic call resolution is in
[CALLS.md](CALLS.md).

**Templates substitute ASTs before ordinary emission.**

- Keep explicit `TemplateArgs` separate from call callee names; canonical type ids
  have no spaces (`Box<Pair<INTEGER,REAL>>`).
- `TemplateClone.cc` owns deep cloning and structural substitution; every new body
  AST node needs coverage there.
- A type parameter binds one canonical type and a pack binds a list; expanded
  parameter names use unlexable `#` suffixes.
- FunctionGen stores overload signatures in `IdentifierMap<std::deque<FuncSig>>` so
  instantiating another template cannot invalidate a held `FuncSig *` during
  marshalling.
- CodeGen owns cloned ASTs for the entire compile and caches each instance by
  declaration plus canonical arguments.

**Template selection and context are shared by typing and emission.**

- When multiple definitions share a function/procedure name, every ordinary and
  template definition must specify OVERLOAD.
- `resolveCall` computes static argument types/ranks once, then tries exact ordinary,
  non-variadic template and variadic template tiers; same-tier ties are errors.
- Candidate checking must stay silent and must not instantiate speculative class
  types. Explicit arguments pre-bind types without allowing conversions.
- Instances keep the template owner for diagnostics/library behavior and the first
  requester in `VisibilityUnit` for lookup (`value_or(CurrentUnit)`).
- Deferred bodies use the symbol snapshot at the template declaration to preserve
  positional own constants; save and restore all caller context.
- Class resolution hooks must honor visibility and reserve/cache the record before
  resolving fields.

**Operator overloads share static selection.**

- OPERATOR is a reserved keyword; `OVERLOAD FUNCTION OPERATOR+(...)` records the
  token on PrototypeAST. Canonical names contain a space (`OPERATOR +`,
  `Class.OPERATOR AND`) to avoid collisions with ordinary identifiers.
- `OperatorGen.cc` probes visible global and left-class member signatures without
  emitting diagnostics or evaluating operands. Matching is exact; member/global ties
  are ambiguous and access is checked only after selection.
- Both getExprTypeInfo and emission must consult this resolver with the expression's
  result context before hardcoded builtin result types. A strict context requires the
  overload's return type to match exactly; a cast only prefers it; without a context,
  return-only-distinct candidates are ambiguous. Returns may be any supported function
  result, including non-BOOLEAN comparisons/logic. emitCoercedExpr still rejects a
  direct STRING to BOOLEAN conversion; CASE requires BOOLEAN comparisons.
- Operators use shared argument marshalling and exception-aware calls. Member
  lvalues keep their original address; rvalues are materialized with Alive tracking
  and destroyed at statement end.

**Assignment operators** are member-only `OVERLOAD PROCEDURE OPERATOR<-` with one
explicit parameter. emitAssignment resolves the exact RHS static type before fallback
storage copying. Internal assignment/constructor BYREF bindings may use designators
or materialized temporaries; ordinary BYREF calls still require bare variables.
