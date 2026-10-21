# File handling and the TYPE system

Part of the [pitfalls index](../PITFALLS.md).

## File handling (§9.1)

- `READ`/`WRITE`/`APPEND`/`RANDOM` are reserved keywords, so `DECLARE READ : INTEGER`
  does not compile.
- `WRITEFILE f, a, b` takes exactly two operands (the stray `, b` is a parse error).
- READFILE's target must be a plain STRING variable — no CHAR, no `arr[i]`.
- The CIE guide's empty-line test `IF LineOfText = ""` compares contents.
- At most 16 files open at once.
- All runtime misuse (not open, wrong mode, reopen, fopen failure, READFILE past EOF)
  throws an anonymous exception — guard reads with `WHILE NOT EOF(...)`.
- Caveat: the open-file table is keyed by the filename *string* (per CIE semantics),
  so two spellings of one OS path (`"a.txt"` vs `"./a.txt"`) evade the already-open
  check and get two independent buffered streams — reads through the alias can see
  stale/empty data instead of an exception.

## TYPE system (§4)

- `TYPE`/`ENDTYPE`/`SET`/`DEFINE` are reserved keywords.
- Typing is *nominal*: two pointer types with the same pointee, or two records with
  the same layout, are incompatible. Strict pointee matching rejects the official
  guide's own worked example (`^INTEGER` pointed at a `Season` variable) —
  deliberate deviation.
- Enum OUTPUT prints the 0-based ordinal; a DECLAREd-but-unset enum *is* the first
  enumerator (no uninitialized state). `E ± INTEGER` needs the enum on the left;
  out-of-range results throw at run time. Enum value names are globally reserved
  (they block DECLARE/parameter/function names).
- There is no NULL literal: pointers zero-init to null, dereferencing one throws, and
  unset-ness is deliberately untestable. `RETURN ^localVar` dangles — documented,
  unchecked.
- Record STRING fields default-construct empty strings and copy independently.
- TYPE must be top-level; array fields inside records and bounded array parameters
  with non-literal bounds are dedicated compile errors.
- A digit run followed by `.` is swallowed into a REAL literal, so `1.foo` mislexes
  (only already-invalid programs can hit it).
