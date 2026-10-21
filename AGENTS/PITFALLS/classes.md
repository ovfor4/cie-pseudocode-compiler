# CLASS subset (§10) and copy limits

Part of the [pitfalls index](../PITFALLS.md).

## Declaration and access

- `CLASS`/`ENDCLASS`/`PUBLIC`/`PRIVATE` are reserved.
- CLASS is a top-level record-derived value type with optional per-member access
  keywords (no DECLARE). Omitting access defaults to public, per the guide. TYPE
  fields cannot specify access and remain public.
- Private fields and methods cannot be accessed outside methods of the owning class;
  whole-value initialization/copying is allowed.
- Methods modify the receiver; `this` aliases the object itself, not a pointer.
  `this <- Other` copies a value.
- Bare fields are supported, but calls to the current object's methods require
  `this.Method(...)`; unqualified calls remain global.
- Parameters and local declarations shadowing fields warn without failing compilation.
- Method-only classes are allowed; empty classes and array fields remain unsupported.

## Construction

- PROCEDURE NEW is an ordinary case-insensitive member, also called automatically
  after zero initialization by DECLARE and assignment-only keyword NEW. Fields and
  array elements recursively require parameterless construction before the outer NEW.
- Class variables and BYVAL class parameters use reference slots: keyword NEW rebinds
  them, while ordinary assignment copies into their current object.
- Fields, array elements, BYREF, this and pointer dereferences receive a value copy
  from NEW; the temporary allocation leaks. Old address-of pointers retain the
  previous object.
- Private access and ordinary argument checks apply to automatic construction too.

## Destruction

- DELETE is reserved; a parameterless PROCEDURE DELETE defines an ordinary method.
  Explicit calls do not recurse into fields or affect Alive tracking.
- Keyword DELETE validates an active allocation start before destroying the object
  and its fields, then frees memory and leaves aliases dangling; FREE skips all
  destruction.
- DECLARE storage and BYVAL copies are destroyed in reverse declaration order at
  normal function exit or when unwinding leaves the frame. Arrays reverse their flat
  element order. Repeated DECLARE destroys its previous incarnation first.
- Lifetimes are function-scoped, even for declarations inside blocks. NEW rebinding
  keeps automatic destruction attached to original storage; heap objects are never
  auto-destroyed. Main objects are destroyed only on normal program end.
- Alive is cleared before destructor calls, preventing retries if a destructor
  throws. Private DELETE access is checked for automatic destruction too.
- A destructor throwing during unwinding propagates and interrupts remaining cleanup
  rather than terminating as C++ would.

## Copy limits

- Classes may define a same-class BYREF copy constructor.
- Classes without one, whole TYPE records containing non-STRING owning class fields,
  and BYVAL arrays of such classes still copy bits; STRING-bearing aggregates
  recursively clone their string-bearing fields, while other owning fields still need
  explicit care.
- Class rvalues transfer ownership when consumed; discarded results and temporary
  receivers are destroyed at statement end.
