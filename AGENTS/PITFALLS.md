# Pitfalls

## Sharp edges (verified, will bite)

Every bullet below was confirmed against the source, most also by compiling and running
test programs.

- **OUTPUT accepts comma-separated expressions (§5.1).** Values are evaluated and
  printed left to right without automatic separators; the final scalar adds a newline.
  Whole arrays and subarrays still print each element on its own line, including in
  mixed output lists; a final array adds no extra newline. INPUT only accepts a plain
  identifier — no `INPUT arr[i]`.
- **Arrays cross function boundaries only as parameters.** `ArrayTable` is scoped per
  function now, so referencing another function's array fails cleanly as
  "Undeclared array". Whole arrays pass as `ARRAY OF T` / `ARRAY[l:u,...] OF T`
  parameters (BYVAL copies the entire buffer and never frees it); a whole-array
  argument against a scalar parameter is still a compile error. FUNCTIONs cannot
  RETURN arrays. A whole-array variable is also rejected as an operand or as a
  value of its element type (it used to leak its raw data pointer).
- **Arrays are additionally block-scoped.** An array DECLAREd inside an
  IF/WHILE/REPEAT/FOR/TRY/CATCH body is not visible after the block ("Undeclared array") —
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
  returns REAL. NUM_TO_STR formats INTEGER with bounded snprintf `"%lld"` (the full signed
  i64 range) and REAL with `"%f"`. REAL buffers cover the full fixed-point double range.
- **`getExprTypeInfo` returns nullptr** for expressions it can't classify (null-check it
  at any new call site) and internally defaults to INTEGER for arithmetic binaries and
  unknown-function calls; OUTPUT reports an error for a nullptr result.
- **CONSTANT** accepts only literals (including negative numeric literals), never expressions or other names. Constants have lexical block scope and may shadow outer names; bindings hiding constants are restored on block exit too. Only previously declared top-level constants are inherited by functions. Constants have no writable storage: assignment, INPUT, READFILE targets, FOR variables, BYREF and address-of are rejected.
- **CASE OF** reads its selector once and tests expression labels in source order, with inclusive TO ranges and an optional final OTHERWISE. Label headers occupy their own source line; bodies may begin after the colon or on subsequent lines. Comparisons reuse ordinary operator rules; records and whole arrays are unsupported.
- The lexer emits no newline tokens; most grammar is keyword-delimited, while CASE
  uses token line information to recognize labels. Bare `RETURN` is recognized
  before block terminators and the next CASE label.
- `verifyModule` runs at the end of `compile()`; invalid modules make the driver
  exit non-zero without writing IR.
- The FOR loop variable must be pre-DECLAREd as INTEGER; FOR reuses that alloca. The
  loop direction is decided at run time from the STEP value's sign; STEP is evaluated
  exactly once, before the loop. (The TO expression is still re-evaluated every
  iteration.)
- **File handling (§9.1) sharp edges.** `READ`/`WRITE`/`APPEND`/`RANDOM` are reserved
  keywords now — `DECLARE READ : INTEGER` no longer compiles. `WRITEFILE f, a, b` takes
  exactly two operands (the stray `, b` is a parse error).
  READFILE's target must be a plain STRING variable — no CHAR, no `arr[i]` (mirrors
  INPUT). The CIE guide's empty-line test `IF LineOfText = ""` compares contents.
  At most 16 files open at once. All runtime misuse (not open, wrong mode, reopen,
  fopen failure, READFILE past EOF) throws an anonymous exception — guard reads with
  `WHILE NOT EOF(...)`. Caveat: the open-file table is keyed by the filename *string*
  (per CIE semantics), so two spellings of one OS path (`"a.txt"` vs `"./a.txt"`) evade
  the already-open check and get two independent buffered streams — reads through the
  alias can see stale/empty data instead of an exception.

- **TYPE-system sharp edges (§4).** `TYPE`/`ENDTYPE`/`SET`/`DEFINE` are reserved
  keywords now. Typing is *nominal*: two pointer types with the same pointee, or two
  records with the same layout, are incompatible. Strict pointee matching rejects the
  official guide's own worked example (`^INTEGER` pointed at a `Season` variable) —
  deliberate deviation. Enum OUTPUT prints the 0-based ordinal; a DECLAREd-but-unset
  enum *is* the first enumerator (no uninitialized state). `E ± INTEGER` needs the
  enum on the left; out-of-range results throw at run time. There is no NULL
  literal: pointers zero-init to null, dereferencing one throws, and unset-ness
  is deliberately untestable. `RETURN ^localVar` dangles — documented, unchecked.
  Record STRING fields stay null until read (normalized to `""` per-load, not at copy
  time). Enum value names are globally reserved (they block DECLARE/parameter/function
  names). TYPE must be top-level; array fields inside records and bounded array
  parameters with non-literal bounds are dedicated compile errors. A digit run
  followed by `.` is swallowed into a REAL literal, so `1.foo` mislexes (only
  already-invalid programs can hit it).

- **CLASS subset (§10).** `CLASS`/`ENDCLASS`/`PUBLIC`/`PRIVATE` are reserved.
  CLASS is a top-level record-derived value type with mandatory per-field access
  keywords (no DECLARE). Unlike the guide's default-public rule, omitting access
  is an error. TYPE fields cannot specify access and remain public. Private fields
  cannot be accessed externally; whole-value initialization/copying is allowed.
  CLASS inherits record limitations, including no array fields or empty bodies.

- **Exception extensions.** TRY requires exactly one parameterless CATCH and
  ENDTRY. Bare THROW raises a new anonymous exception even inside CATCH; there is
  no rethrow syntax, error object or what(). Runtime checks are catchable;
  compiler errors are not. Runtime checks print the source line and error reason
  to stdout before throwing, even when caught. Uncaught exceptions also print the
  generic termination message and return 1. Bare THROW prints no diagnostic itself.
  Native binaries must link the C++ runtime with LLVM 23's `clang++`
  (`clang++-23` on Debian/Ubuntu).
  Unwinding does not free heap strings/arrays or close files. Linux LLVM 23 is
  verified; Windows and macOS exception execution is not.

- **C type extensions.** C_STRING is a raw char pointer, not a STRING: explicitly
  cast to STRING for printing or builtins. C aliases share canonical CIdentity;
  do not use LLVM widths to recover signedness or nominal type identity. Plain
  char's signedness is native, unlike pseudocode CHAR. C/pseudocode conversions
  require casts even for literals. C scalar arithmetic uses native promotions,
  but / always returns REAL. Cast aliases must precede their use. long double
  may have padding; sizeof does not identify its floating-point format. Native
  C support is verified on Linux LLVM 23; cross-target and EXTERN are unsupported.

## Not implemented (9618 features)

SET types (`SET`/`DEFINE` are reserved with dedicated errors),
random files (OPENFILE...FOR RANDOM/SEEK/GETRECORD/PUTRECORD — TYPE is done, so these
are unblocked now; the RANDOM keyword is reserved and gives a dedicated parse error),
DATE, INPUT into array elements, string escape sequences,
array-typed record fields, whole-array RETURN, class methods/constructors/destructors,
NEW and class inheritance. Compiler fixtures now live in `tests/` (see TESTING.md).
Nightly CI runs the compiler suites, packaged EMBEDDED smoke checks and existing
plugin tests. The initial compiler suite intentionally exposes existing standard
conflicts and missing diagnostic locations; see `tests/FAILURES.md` before interpreting
a failing baseline. Keep those failures visible until their implementations are fixed.
