# Class construction, destruction and copies

Part of the [subsystem notes](../SUBSYSTEMS.md). Builds on [classes](CLASSES.md); see also
[explicit memory](MEMORY.md) and [exceptions](RUNTIME_CHECKS_EXCEPTIONS.md).

## Construction

- `ConstructorGen.cc` emits recursive field construction in declaration order followed
  by the ordinary NEW method, sharing `emitMethodCall` access checks, argument
  marshalling and exception-aware calls.
- DECLARE zeroes its original storage on every execution; arrays loop over all flat
  elements and default-construct each one.
- Constructor argument lists are class-only. Plain DECLARE, field and array defaults go
  through `emitDefaultConstruct`, which calls NEW only when a parameterless one exists
  (else the object stays zeroed with constructed fields; guide 10.2); DECLARE with
  parentheses uses `emitConstruct`. Explicit NEW method calls do not reconstruct fields.
- `SymbolInfo::IsReference` marks a pointer slot for declared class variables and BYVAL
  class parameters; `symbolAddress` loads their current object address for all value,
  lvalue, address-of, receiver and BYREF paths. Field aliases, this and BYREF
  parameters bind addresses directly.
- Assignment-only `NewExprAST` uses checked memory-library allocation (at least one byte
  for method-only classes), zero initialization and construction, then rebinds
  reference slots or copies into other destinations.
- Ordinary assignment invokes an exact member OPERATOR<- when available, otherwise it
  copies.
- Heap objects require explicit keyword DELETE; INHERITS/SUPER remain unsupported.

## Destruction

- `ConstructorGen.cc` also owns `typeNeedsDestruction` and recursive `emitDestroy`:
  ordinary DELETE method first, then fields in reverse declaration order. Explicit
  DELETE method calls remain ordinary calls without field recursion.
- Keyword DELETE resolves a class lvalue, validates its allocation start through
  the memory-library validator before any destruction, destroys it, then releases it
  through the same runtime as FREE;
  aliases remain dangling.
- Stack lifetimes use a per-function `StackObjects` list saved/restored around function
  emission. DECLARE and BYVAL copies share ownership registration, including arrays and
  records with class fields.
- Each record retains its original source line and owning unit so deferred cleanup
  uses the correct __LIB region, even for globals from another unit. It also retains
  the original address, an entry-initialized Alive flag and, for
  arrays, a runtime element count in an entry slot. Array buffer addresses also live in
  entry slots so deferred cleanup never references branch-local bound SSA values.
- Every array registers, even when its elements need no destruction: cleanup runs the
  element-destroy loop only when needed, then `free`s the buffer and nulls the slot, so
  re-DECLARE, RETURN, unwinding and BYVAL copies all release it. BYREF arrays are never
  registered.
- Repeated DECLARE destroys its previous live incarnation first. Alive becomes true only
  after construction or copy completes and is cleared before destruction.
- Rebinding a reference slot with NEW does not change the original address tracked for
  automatic destruction.

## Class copies and temporaries

- Callers construct BYVAL class lvalue arguments with a same-class BYREF OVERLOAD NEW
  when defined, otherwise they copy bits; the callee's RegisterCopy hook takes
  ownership.
- emitOwnedValue constructs lvalue copies and keeps pending arguments Alive until
  marshalling succeeds. RETURN copies lvalues and transfers rvalues.
- Records and BYVAL arrays recursively clone STRING-bearing fields/elements. Other
  owning class fields are copied shallowly (see the
  [copy limits](../PITFALLS/classes.md#copy-limits)).
- StackObject::Temporary uses statement cleanup and deferred temporary-only cleanup
  before local handlers, while ordinary objects remain function-scoped.
- Function-result method receivers are materialized by the same path as operator
  receivers.
- Assignment first selects an exact member OPERATOR<-; without one, STRING-bearing
  aggregate assignment clones string fields from lvalues or transfers them from rvalues,
  releasing the target's old string fields without invoking the enclosing class
  destructor; other classes are assigned bitwise.
