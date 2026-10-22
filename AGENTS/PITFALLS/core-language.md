# Core language pitfalls

Part of the [pitfalls index](../PITFALLS.md). Every bullet was confirmed against the
source, most also by compiling and running test programs.

- **OUTPUT accepts comma-separated expressions (§5.1).** Values are evaluated and
  printed left to right without automatic separators; the final scalar adds a newline.
  Whole arrays and subarrays still print each element on its own line, including in
  mixed output lists; a final array adds no extra newline. INPUT accepts writable
  scalar locations, including array elements, public record/class fields and
  pointer dereferences, using the shared lvalue checks. Whole arrays and whole
  records/classes remain unsupported INPUT targets.
- **Arrays cross function boundaries only as parameters.** `ArrayTable` is scoped per
  function, so referencing another function's array fails cleanly as
  "Undeclared array". Whole arrays pass as `ARRAY OF T` / `ARRAY[l:u,...] OF T`
  parameters (BYVAL copies the entire buffer and never frees it); a whole-array
  argument against a scalar parameter is still a compile error. FUNCTIONs cannot
  RETURN arrays. A whole-array variable is also rejected as an operand or as a
  value of its element type.
- **Arrays are additionally block-scoped.** An array DECLAREd inside an
  IF/WHILE/REPEAT/FOR/TRY/CATCH body is not visible after the block ("Undeclared
  array") — its buffer setup and bound SSA values live inside the branch, so letting
  it escape would mean wild-pointer writes or invalid IR. Scalars remain function-scoped
  (their alloca is hoisted to the entry block); declare at the top of the function
  like the exam board intends and neither matters.
- **Parameter modes are sticky and BYREF is FUNCTION-forbidden (§8.3).**
  `(BYREF a : T, b : T)` makes `b` BYREF too. Literal arguments against an
  accidentally-BYREF parameter fail with "BYREF argument must be a variable";
  annotate `BYVAL` explicitly.
- **`/` always yields REAL** (double FDiv), even for two INTEGERs; integer division is
  DIV/MOD only.
  - CHR returns an owning one-byte STRING, not CHAR; CHR(0) retains one NUL byte.
  - STR_TO_NUM always returns REAL and shares parseNumber (strtod, full consumption, overflow rejected) with IS_NUM.
  - NUM_TO_STR uses std::to_string, preserving the full signed i64 range and six
    fractional digits for REAL.
- **`getExprTypeInfo` returns nullptr** for expressions it can't classify (null-check
  it at any new call site) and internally defaults to INTEGER for unclassified
  arithmetic expressions; unknown or unmatched calls diagnose and return nullptr.
  OUTPUT reports an error for a nullptr result.
- **CONSTANT** accepts only literals (including negative numeric literals), never
  expressions or other names. Constants have lexical block scope and may shadow outer
  names; bindings hiding constants are restored on block exit too. Functions inherit
  previously declared constants from their own unit and all visible imported
  top-level constants. Constants have no writable storage: assignment, INPUT,
  READFILE targets, FOR variables, BYREF and address-of are rejected.
- **CASE OF** reads its selector once and tests expression labels in source order,
  with inclusive TO ranges and an optional final OTHERWISE. Label headers occupy their
  own source line; bodies may begin after the colon or on subsequent lines.
  Comparisons reuse ordinary operator rules; records/classes need BOOLEAN-returning
  comparison overloads, and whole arrays are unsupported.
- The lexer emits no newline tokens; most grammar is keyword-delimited, while CASE
  uses token line information to recognize labels. Bare `RETURN` is recognized
  before block terminators and the next CASE label.
- `verifyModule` runs at the end of `compile()`; invalid modules make the driver
  exit non-zero without writing IR.
- The FOR loop variable must be pre-DECLAREd as INTEGER; FOR reuses that alloca. The
  loop direction is decided at run time from the STEP value's sign; STEP is evaluated
  exactly once, before the loop. (The TO expression is still re-evaluated every
  iteration.)
- WHILE accepts both the standard header without DO and the older WHILE DO form.
- **Literal escapes.** C++17 escapes are decoded by the lexer. Unicode escapes encode
  UTF-8 in strings; CHAR Unicode escapes must be ASCII. Ordinary STRING literals
  retain legacy NUL truncation: `"a\0b"` is observed as `"a"`. STRING can retain NUL
  bytes built through CHR or the count/character constructor; raw C text I/O
  truncates at NUL. The suffix `"text"C` yields C_STRING without a STRING class.
