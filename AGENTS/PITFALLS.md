# Pitfalls

## Sharp edges (verified, will bite)

Every bullet below was confirmed against the source, most also by compiling and running
test programs.

- **STRING `=` / `<>` compares pointer identity, not contents.** No strcmp anywhere;
  two equal heap strings compare unequal.
- **Descending FOR loops never work.** CodeGen picks the loop direction at compile time
  by dynamic_casting the STEP to a negative `IntegerExprAST` — but `-1` parses as the
  expression `0 - 1` (the lexer has no signed literals), so that branch is dead code and
  *every* FOR compiles with an ascending `<=` test. `FOR i <- 5 TO 1 STEP -1` runs zero
  iterations (empirically verified).
- **OUTPUT takes exactly one expression.** `OUTPUT a, ", ", b` silently drops everything
  after the comma — and if the dropped tail ends in an identifier, error recovery also
  swallows the first token of the *next* statement. Concatenate with `&`. Similarly
  INPUT only accepts a plain identifier — no `INPUT arr[i]`.
- **Arrays cannot cross function boundaries.** `ArrayTable` is never scoped; metadata
  SSA values belong to the declaring function, so referencing another function's array
  produces invalid IR, and arrays can't be parameters.
- **`/` always yields REAL** (double FDiv), even for two INTEGERs; integer division is
  DIV/MOD only. CHR returns a STRING (2-byte heap buffer), not CHAR. STR_TO_NUM always
  returns REAL. NUM_TO_STR formats i64 with sprintf `"%d"` — corrupts values over 32
  bits.
- **`getExprTypeInfo` returns nullptr** for expressions it can't classify (null-check it
  at any new call site) and internally defaults to INTEGER for arithmetic binaries and
  unknown-function calls; OUTPUT substitutes INTEGER for a nullptr result — so wrong
  inference prints garbage instead of erroring.
- The lexer emits no newline tokens; the grammar is purely keyword-delimited. Bare
  `RETURN` is only recognized right before ENDIF/ELSE/ENDFUNCTION/ENDPROCEDURE.
- `verifyFunction` results are ignored; invalid modules still print.
- The FOR loop variable must be pre-DECLAREd as INTEGER; FOR reuses that alloca.

## Not implemented (9618 features)

CASE OF, CONSTANT, TYPE/ENDTYPE (records/enums/pointers), file handling
(OPENFILE/READFILE/WRITEFILE/CLOSEFILE/EOF), DATE, multi-expression OUTPUT, INPUT into
array elements, string escape sequences. `README.md` lists `type` and `file` as the
intended next work. No tests and no CI exist; verify changes by compiling sample
programs end-to-end (command sequence in `ARCH.md`).
