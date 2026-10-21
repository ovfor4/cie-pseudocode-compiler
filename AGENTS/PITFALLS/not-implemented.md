# Not implemented (9618 features)

Part of the [pitfalls index](../PITFALLS.md).

- SET types (`SET`/`DEFINE` are reserved with dedicated errors).
- Random files (OPENFILE...FOR RANDOM/SEEK/GETRECORD/PUTRECORD — TYPE is done, so
  these are unblocked now; the RANDOM keyword is reserved and gives a dedicated parse
  error).
- DATE.
- Array-typed record fields, whole-array RETURN.
- Class inheritance (INHERITS/SUPER).

## Status notes

- Compiler fixtures now live in `tests/` (see [TESTING.md](../TESTING.md)).
- Nightly CI runs the compiler suites, packaged EMBEDDED smoke checks and existing
  plugin tests.
- The initial standard conflicts and missing diagnostic locations are fixed on Linux.
