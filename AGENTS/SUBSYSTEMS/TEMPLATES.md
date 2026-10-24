# Templates

Part of the [subsystem notes](../SUBSYSTEMS.md). Related: [functions](FUNCTIONS.md),
[classes](CLASSES.md).

## Declarations and validation

`TemplateDeclAST` owns a function/procedure or class definition plus type parameters;
`PackExpansionExprAST` represents a value pack used in an argument list. The top-level
name scan registers templates without treating them as executable code. After ordinary
prototype registration, `validateTemplates` checks OVERLOAD consistency across all
units, including invisible definitions. CONSTANT/GLOBAL name checks use template-aware
callable lookup.

## Call resolution

- `CodeGen::resolveCall` retains ordinary fast paths when no templates exist. Otherwise
  it computes static argument types and whole-array ranks once, selecting exact ordinary
  definitions (including an expected result type when present) before non-variadic
  templates and then variadic templates. Templates still match only substituted
  parameter types; result contexts do not filter them.
- Explicit type arguments pre-bind positional parameters and constrain a pack prefix;
  deduction from `T` or array element `T` fills remaining bindings. Composite types such
  as `Box<T>` are checked after substitution but do not deduce T.
- Every position must match exactly and every non-pack type must be bound. Candidate
  failures stay silent; only the final no-match or ambiguity is diagnosed.
- `hasCallable` keeps CALL statement re-resolution aware of templates.

## Cloning (`TemplateClone.{h,cc}`)

- Deep-clones body ASTs with structural type substitution, preserving source lines.
- Bindings map each type parameter to a list (one type for ordinary parameters). A final
  value pack expands into `Name#1` through `Name#N`; argument-list cloning expands
  references only in calls, member calls, NEW and DECLARE constructors.
- Class cloning substitutes fields, method types and owning class names; pack fields and
  nested function definitions are rejected.
- Add clone coverage whenever introducing a body AST node.

## Instantiation (`TemplateGen.cc`)

- Caches instances by source declaration and canonical type arguments, owns cloned ASTs
  for the compile lifetime and immediately registers selected function prototypes.
- Names are `Max<INTEGER>`, with the parameter signature appended for OVERLOAD.
  FunctionGen's signature deques keep pointers stable while recursive instantiation
  appends signatures.
- `TypeSystem::resolve` has a miss hook for visible class templates, including
  diagnostics for bare template names. It resolves arguments, reserves and caches an
  opaque record, defines its fields, records the template owner in TypeOwners and uses
  the factored `registerClassMethods`/`emitClassPrototypes` paths.
- Prototypes requested during the type pre-pass are deferred until it completes.
- Failed type names are remembered to avoid repeating the instantiation error;
  speculative candidate checks do not record failures.
- `findMethod` forces type resolution for template ids before NEW/DELETE lookup.

## Body emission

- After all units' statement loops, `drainTemplateBodies` emits queued functions and
  class methods through `emitStmt`, repeating for newly requested instances until the
  queue is empty. Instantiation depth is limited to 64.
- `selectUnit` restores the template owner's file, library mode and call hook;
  `VisibilityUnit` uses the first requester's transitive visibility, including
  requester-defined operators needed by a library template.
- Each body receives the symbol snapshot from the template declaration, preserving
  positional own constants and imported constants. SaveAndRestore guards and unit
  restoration retain the caller's state.
