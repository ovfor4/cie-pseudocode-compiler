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
- **Arrays cannot cross function boundaries.** `ArrayTable` is never scoped; metadata
  SSA values belong to the declaring function, so referencing another function's array
  produces invalid IR. Passing a whole array as a call argument is rejected with a
  compile error.
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

## Not implemented (9618 features)

CASE OF, CONSTANT, TYPE/ENDTYPE (records/enums/pointers), random files
(OPENFILE...FOR RANDOM/SEEK/GETRECORD/PUTRECORD — blocked on TYPE; the RANDOM keyword
is reserved and gives a dedicated parse error), DATE, multi-expression OUTPUT, INPUT
into array elements, string escape sequences. `README.md` lists `type` as the intended
next work. No tests and no CI exist; verify changes by compiling sample programs
end-to-end (command sequence in `ARCH.md`).
