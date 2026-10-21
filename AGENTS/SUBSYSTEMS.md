# Subsystem notes

Deep-dives on the CodeGen helper subsystems, split by topic under `SUBSYSTEMS/`.
(Invariants: `CONVENTIONS.md`; known bugs: `PITFALLS.md`.)

- [Arrays](SUBSYSTEMS/ARRAYS.md): `ArrayHandler` storage, checked indexing, array
  parameters and array OUTPUT.
- [Functions and operator overloads](SUBSYSTEMS/FUNCTIONS.md): `FunctionGen`
  prototypes, parameter modes, OVERLOAD resolution, `OperatorGen`.
- [EXTERN "C" and INCLUDE_OBJ](SUBSYSTEMS/EXTERN.md): external prototypes, variadic
  calls, driver object-symbol handling.
- [User types](SUBSYSTEMS/USER_TYPES.md): `TypeSystem`, two-pass TYPE registration,
  lvalues, admission, enum ordinals.
- [Classes](SUBSYSTEMS/CLASSES.md): CLASS declarations, field access, methods, `this`,
  member calls.
- [Class construction, destruction and copies](SUBSYSTEMS/CLASS_LIFETIME.md):
  `ConstructorGen`, DELETE, stack-object ownership, BYVAL copies, temporaries.
- [Runtime checks and exceptions](SUBSYSTEMS/RUNTIME_CHECKS_EXCEPTIONS.md):
  `RuntimeCheck`, TRY/CATCH/THROW lowering, cleanup pads, EH runtime per platform.
- [Files](SUBSYSTEMS/FILES.md): `FileHandler` text-file runtime and open-file table.
- [Explicit memory](SUBSYSTEMS/MEMORY.md): `MemoryHandler` allocation tracking,
  MALLOC/REALLOC/FREE.
- [Standard library](SUBSYSTEMS/STDLIB.md): `pseudocode-lib/`, embedding, STRING class.
- [Templates](SUBSYSTEMS/TEMPLATES.md): template declarations, resolution, cloning,
  instantiation, body emission.
