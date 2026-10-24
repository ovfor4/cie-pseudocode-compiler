# OVERLOAD, TEMPLATE and OPERATOR extensions

Part of the [pitfalls index](../PITFALLS.md).

## OVERLOAD

- OVERLOAD is reserved and prefixes FUNCTION/PROCEDURE (after PUBLIC/PRIVATE for
  methods). Every same-name definition must opt in.
- Count, canonical parameter types, array rank and (for non-VOID definitions) return
  type distinguish signatures; a PROCEDURE counts as VOID. BYREF/BYVAL and array
  bounds do not.
- Calls match exact static types, even for a single OVERLOAD definition; ordinary
  functions still coerce arguments. A known expected result type (assignment, BYVAL
  argument, RETURN, condition, subscript, CASE comparison) must match the overload's
  return type exactly; `(T) x` only prefers T and otherwise converts. Without a
  context, return-only-distinct candidates are ambiguous.
- Call visibility filters the set, but duplicate signatures (parameters and return
  type) conflict across all inputs. MAIN and builtins cannot be overloaded.
- NEW supports overloads; DECLARE, fields and arrays needing default construction
  require a matching no-argument NEW.
- Private access is checked after selection, never used to select a public fallback.

## TEMPLATE

- Top-level FUNCTION/PROCEDURE/CLASS only; all template parameters are types, with at
  most one final pack.
- Exact types are required even with explicit arguments (`Max<REAL>(1, 2.5)` fails).
  Deduction recognizes T and array element T, never the T inside Box<T>.
- When multiple ordinary/template functions share a name, every definition must
  specify OVERLOAD; exact ordinary definitions win, then non-variadic templates, then
  variadic templates, with same-tier ties ambiguous. Explicit arguments select
  templates only.
- Packs are final non-array parameters and expand only in call/constructor argument
  lists; pack fields are rejected, including in variadic classes.
- Unsupported: non-type parameters, SIZEOF..., member/operator templates,
  specialization, bare injected class name, pointer-alias templates. Pointer-linked
  generic containers remain unsupported; use fixed fields or other supported value
  layouts.
- Class instances follow ordinary constructor rules, so a parameterized NEW alone does
  not permit `DECLARE B : Box<INTEGER>` without arguments.
- Template instances share the first requester's visibility for their cached body;
  positional constants come from the template declaration. Instantiation depth is
  limited to 64.
- Lexing: empty source lists `<>` are invalid and lex as inequality; `>>` remains two
  tokens, but adjacent `<-` and `>=` merge. Expression lookahead treats
  `A < B, C > (D)` as a template call; parenthesize comparisons to avoid that
  ambiguity.

## OPERATOR

- Unary/binary operators require OVERLOAD FUNCTION, with supported arity and at least
  one record/class/enum operand. Pointer aliases alone do not qualify.
- Exact matching considers visible globals and left-class members together, and the
  result type joins the match when the context supplies one; ties are errors,
  including private/public ties. Without a context, return-only-distinct candidates
  are ambiguous; casts only prefer their target type.
- Comparison and logic result types are declared, not assumed BOOLEAN; a strict
  BOOLEAN context requires an overload returning BOOLEAN. CASE comparisons must return
  BOOLEAN. AND/OR overloads evaluate both operands.
- Temporary member receivers are destroyed at statement end, including exception
  exits; ordinary DECLARE objects remain function-scoped.
- Explicit calls use OPERATOR+(a,b) or a.OPERATOR+(b).
- Assignment uses member-only OVERLOAD PROCEDURE OPERATOR<- with one explicit
  parameter; it is selected before fallback same-class copying. Keyword NEW retains
  its allocation/rebinding path.
