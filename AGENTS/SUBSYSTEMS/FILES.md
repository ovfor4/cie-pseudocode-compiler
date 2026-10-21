# Files

Part of the [subsystem notes](../SUBSYSTEMS.md). Related:
[runtime checks and exceptions](RUNTIME_CHECKS_EXCEPTIONS.md).

**Files** (`FileHandler`): text-file runtime for §9.1.

## Structure

- `FileHandler.cc` owns setup, lookup/open and emission entry points;
  `FileRuntimeIO.cc` builds the read/write/close/EOF helpers.
- Leaf helper — holds Context/Builder/Module/RuntimeCheck/ExceptionHandler refs only,
  never sees CodeGen, AST nodes or the symbol table.
- `CodeGen::emitStmtImpl`'s four file branches (and the EOF builtin intercept)
  evaluate/coerce operands and hand over `llvm::Value*`s plus the statement line.
- Calls into throwing helpers go through ExceptionHandler.

## Synthesized runtime

- On first file use, `ensureRuntime()` synthesizes the whole runtime once — the file
  module-level helpers: internal-linkage `__cps_file_find/open/read/write/close/eof`
  plus a 16-slot open-file table (`__cps_file_names/handles/modes` parallel globals;
  mode byte 0 = free).
- File-free programs get byte-identical IR — nothing is declared or emitted eagerly.
- Helper bodies are built with the *shared* Builder (RuntimeCheck emits through the same
  one), saving/restoring the insert block.

## Semantics

- Files are keyed by filename *content* (strcmp scan), consistent with STRING
  comparison; the table owns a copied filename until CLOSEFILE, independently of
  temporary STRING lifetimes.
- EOF is an fgetc/ungetc peek; READFILE grows a malloc/realloc buffer per line and
  strips one trailing `\r`.
- fopen modes are text `"r"/"w"/"a"` (Windows CRT translates CRLF).
