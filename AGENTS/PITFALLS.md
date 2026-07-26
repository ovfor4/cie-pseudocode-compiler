# Pitfalls

## Sharp edges (verified, will bite)

Every bullet below was confirmed against the source, most also by compiling and running
test programs.

- **STRING `=` / `<>` compares pointer identity, not contents.** No strcmp anywhere;
  two equal heap strings compare unequal.
- **OUTPUT takes exactly one expression.** `OUTPUT a, ", ", b` prints `a`, then the
  stray `, ", ", b` produces parse errors (recovery resyncs at the next statement
  start, so following statements survive; the compile still fails). Concatenate with
  `&`. Similarly INPUT only accepts a plain identifier — no `INPUT arr[i]`.
- **Arrays cross function boundaries only as parameters.** `ArrayTable` is scoped per
  function now, so referencing another function's array fails cleanly as
  "Undeclared array". Whole arrays pass as `ARRAY OF T` / `ARRAY[l:u,...] OF T`
  parameters (BYVAL copies the entire buffer and never frees it); a whole-array
  argument against a scalar parameter is still a compile error. FUNCTIONs cannot
  RETURN arrays. A whole-array variable is also rejected as an operand or as a
  value of its element type (it used to leak its raw data pointer).
- **Arrays are additionally block-scoped.** An array DECLAREd inside an
  IF/WHILE/REPEAT/FOR body is not visible after the block ("Undeclared array") —
  its buffer setup and bound SSA values live inside the branch, so letting it
  escape meant wild-pointer writes or invalid IR. Scalars remain function-scoped
  (their alloca is hoisted to the entry block); declare at the top of the
  function like the exam board intends and neither matters.
- **Parameter modes are sticky and BYREF is FUNCTION-forbidden (§8.3).**
  `(BYREF a : T, b : T)` makes `b` BYREF too — programs that relied on the old
  per-parameter reset (where `b` was silently BYVAL) change meaning. Literal
  arguments against an accidentally-BYREF parameter now fail with "BYREF argument
  must be a variable"; annotate `BYVAL` explicitly.
- **`/` always yields REAL** (double FDiv), even for two INTEGERs; integer division is
  DIV/MOD only. CHR returns a STRING (2-byte heap buffer), not CHAR. STR_TO_NUM always
  returns REAL. NUM_TO_STR formats i64 with sprintf `"%d"` — corrupts values over 32
  bits.
- **`getExprTypeInfo` returns nullptr** for expressions it can't classify (null-check it
  at any new call site) and internally defaults to INTEGER for arithmetic binaries and
  unknown-function calls; OUTPUT reports an error for a nullptr result.
- The lexer emits no newline tokens; the grammar is purely keyword-delimited. Bare
  `RETURN` is only recognized right before ENDIF/ELSE/ENDFUNCTION/ENDPROCEDURE.
- Invalid modules still print their IR, but `verifyModule` runs at the end of
  `compile()` and makes the driver exit non-zero.
- The FOR loop variable must be pre-DECLAREd as INTEGER; FOR reuses that alloca. The
  loop direction is decided at run time from the STEP value's sign; STEP is evaluated
  exactly once, before the loop. (The TO expression is still re-evaluated every
  iteration.)
- **File handling (§9.1) sharp edges.** `READ`/`WRITE`/`APPEND`/`RANDOM` are reserved
  keywords now — `DECLARE READ : INTEGER` no longer compiles. `WRITEFILE f, a, b` takes
  exactly two operands (the stray `, b` is a parse error, like multi-expression OUTPUT).
  READFILE's target must be a plain STRING variable — no CHAR, no `arr[i]` (mirrors
  INPUT). The CIE guide's empty-line test `IF LineOfText = ""` silently fails here
  because STRING `=` is pointer identity — use `LENGTH(LineOfText) = 0`. At most 16
  files open at once. All runtime misuse (not open, wrong mode, reopen, fopen failure,
  READFILE past EOF) is a `[Fatal] line %d:` exit(1), by design — guard reads with
  `WHILE NOT EOF(...)`. Caveat: the open-file table is keyed by the filename *string*
  (per CIE semantics), so two spellings of one OS path (`"a.txt"` vs `"./a.txt"`) evade
  the already-open check and get two independent buffered streams — reads through the
  alias can see stale/empty data instead of a Fatal.

- **TYPE-system sharp edges (§4).** `TYPE`/`ENDTYPE`/`SET`/`DEFINE` are reserved
  keywords now. Typing is *nominal*: two pointer types with the same pointee, or two
  records with the same layout, are incompatible. Strict pointee matching rejects the
  official guide's own worked example (`^INTEGER` pointed at a `Season` variable) —
  deliberate deviation. Enum OUTPUT prints the 0-based ordinal; a DECLAREd-but-unset
  enum *is* the first enumerator (no uninitialized state). `E ± INTEGER` needs the
  enum on the left; out-of-range results die `[Fatal]` at run time. There is no NULL
  literal: pointers zero-init to null, dereferencing one dies `[Fatal]`, and unset-ness
  is deliberately untestable. `RETURN ^localVar` dangles — documented, unchecked.
  Record STRING fields stay null until read (normalized to `""` per-load, not at copy
  time). Enum value names are globally reserved (they block DECLARE/parameter/function
  names). TYPE must be top-level; array fields inside records and bounded array
  parameters with non-literal bounds are dedicated compile errors. A digit run
  followed by `.` is swallowed into a REAL literal, so `1.foo` mislexes (only
  already-invalid programs can hit it).

## Not implemented (9618 features)

CASE OF, CONSTANT, SET types (`SET`/`DEFINE` are reserved with dedicated errors),
random files (OPENFILE...FOR RANDOM/SEEK/GETRECORD/PUTRECORD — TYPE is done, so these
are unblocked now; the RANDOM keyword is reserved and gives a dedicated parse error),
DATE, multi-expression OUTPUT, INPUT into array elements, string escape sequences,
array-typed record fields, whole-array RETURN. No tests and no CI exist; verify
changes by compiling sample programs end-to-end (command sequence in `ARCH.md`).
