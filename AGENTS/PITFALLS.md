# Pitfalls

## Sharp edges (verified, will bite)

Every bullet below was confirmed against the source, most also by compiling and running
test programs.

- **Multi-file extensions.** INCLUDE imports symbols, never text; every included file
  must also be an input. Duplicate exports across files fail even without visibility, except distinct
  signatures whose definitions all specify OVERLOAD.
  Only one file may contain top-level code; top-level DECLARE is a main local, not an
  export. Definitions-only builds now fail without FUNCTION MAIN returning INTEGER.
  MAIN cannot coexist with top-level code. GLOBAL accepts only the five primitive
  scalar types and literal initializers; arrays, user types and computed initializers
  are rejected. Own top-level constants remain positional, imported constants do not.
- **OUTPUT accepts comma-separated expressions (§5.1).** Values are evaluated and
  printed left to right without automatic separators; the final scalar adds a newline.
  Whole arrays and subarrays still print each element on its own line, including in
  mixed output lists; a final array adds no extra newline. INPUT accepts writable
  scalar locations, including array elements, public record/class fields and
  pointer dereferences, using the shared lvalue checks. Whole arrays and whole
  records/classes remain unsupported INPUT targets.
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
  at any new call site) and internally defaults to INTEGER for unclassified arithmetic expressions; unknown or unmatched calls now diagnose and return nullptr.
  OUTPUT reports an error for a nullptr result.
- **CONSTANT** accepts only literals (including negative numeric literals), never expressions or other names. Constants have lexical block scope and may shadow outer names; bindings hiding constants are restored on block exit too. Functions inherit previously declared constants from their own unit and all visible imported top-level constants. Constants have no writable storage: assignment, INPUT, READFILE targets, FOR variables, BYREF and address-of are rejected.
- **CASE OF** reads its selector once and tests expression labels in source order, with inclusive TO ranges and an optional final OTHERWISE. Label headers occupy their own source line; bodies may begin after the colon or on subsequent lines. Comparisons reuse ordinary operator rules; records/classes need BOOLEAN-returning comparison overloads, and whole arrays are unsupported.
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
  READFILE's target must be a plain STRING variable — no CHAR, no `arr[i]`.
  The CIE guide's empty-line test `IF LineOfText = ""` compares contents.
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

- **OVERLOAD extension.** OVERLOAD is reserved and prefixes FUNCTION/PROCEDURE
  (after PUBLIC/PRIVATE for methods). Every same-name definition must opt in.
  Only count, canonical parameter types and array rank distinguish signatures:
  return type, BYREF/BYVAL and array bounds do not. Calls match exact static types,
  even for a single OVERLOAD definition; ordinary functions still coerce arguments.
  Call visibility filters the set, but duplicate signatures conflict across all
  inputs. MAIN and builtins cannot be overloaded. NEW supports overloads; DECLARE,
  fields and arrays needing default construction require a matching no-argument NEW.
  Private access is checked after selection, never used to select a public fallback.
- **OPERATOR extension.** Requires OVERLOAD FUNCTION, with supported unary/binary
  arity and at least one record/class/enum operand. Pointer aliases alone do not
  qualify. Exact matching considers visible globals and left-class members
  together; ties are errors, including private/public ties. Comparison and logic
  result types are declared, not assumed BOOLEAN; implicit STRING to BOOLEAN is
  rejected. CASE comparisons must return BOOLEAN. AND/OR overloads evaluate both
  operands. Temporary member receivers have function-scoped cleanup, not C++
  full-expression lifetimes. Explicit calls use OPERATOR+(a,b) or a.OPERATOR+(b).
- **CLASS subset (§10).** `CLASS`/`ENDCLASS`/`PUBLIC`/`PRIVATE` are reserved.
  CLASS is a top-level record-derived value type with optional per-member access
  keywords (no DECLARE). Omitting access defaults to public, per the guide.
  TYPE fields cannot specify access and remain public. Private fields
  and methods cannot be accessed outside methods of the owning class;
  whole-value initialization/copying is allowed. Methods modify the receiver;
  `this` aliases the object itself, not a pointer. `this <- Other` copies a value.
  Bare fields are supported, but calls to the current object's methods require
  `this.Method(...)`; unqualified calls remain global. Parameters and local
  declarations shadowing fields warn without failing compilation. Method-only
  classes are allowed; empty classes and array fields remain unsupported.
  PROCEDURE NEW is an ordinary case-insensitive member, also called automatically
  after zero initialization by DECLARE and assignment-only keyword NEW. Fields and
  array elements recursively require parameterless construction before the outer NEW.
  Class variables and BYVAL class parameters use reference slots: keyword NEW rebinds
  them, while ordinary assignment copies into their current object. Fields, array
  elements, BYREF, this and pointer dereferences receive a value copy from NEW;
  the temporary allocation leaks. Old address-of pointers retain the previous object.
  Private access and ordinary argument checks apply to automatic construction too.
  DELETE is reserved; a parameterless PROCEDURE DELETE defines an ordinary method.
  Explicit calls do not recurse into fields or affect Alive tracking. Keyword DELETE
  validates an active allocation start before destroying the object and its fields,
  then frees memory and leaves aliases dangling; FREE skips all destruction.
  DECLARE storage and shallow BYVAL copies are destroyed in reverse declaration order
  at normal function exit or when unwinding leaves the frame. Arrays reverse their
  flat element order. Repeated DECLARE destroys its previous incarnation first.
  Lifetimes are function-scoped, even for declarations inside blocks. NEW rebinding
  keeps automatic destruction attached to original storage; heap objects are never
  auto-destroyed. Main objects are destroyed only on normal program end. Alive is
  cleared before destructor calls, preventing retries if a destructor throws.
  Private DELETE access is checked for automatic destruction too. A destructor
  throwing during unwinding propagates and interrupts remaining cleanup rather
  than terminating as C++ would.

- **Exception extensions.** TRY requires exactly one parameterless CATCH and
  ENDTRY. Bare THROW raises a new anonymous exception even inside CATCH; there is
  no rethrow syntax, error object or what(). Runtime checks are catchable;
  compiler errors are not. Runtime checks print the source line and error reason
  to stderr before throwing, even when caught. Uncaught exceptions also print the
  generic termination message and return 1. Bare THROW prints no diagnostic itself.
  Native binaries must link the C++ runtime with LLVM 23's `clang++`
  (`clang++-23` on Debian/Ubuntu).
  Unwinding does not free heap strings/arrays or close files. Embedded ORC
  execution with LLVM 23 is verified on Linux, Windows and macOS.

- **C type extensions.** C_STRING is a raw char pointer, not a STRING: explicitly
  cast to STRING for printing or builtins. C aliases share canonical CIdentity;
  do not use LLVM widths to recover signedness or nominal type identity. Plain
  char's signedness is native, unlike pseudocode CHAR. C/pseudocode conversions
  require casts even for literals. C scalar arithmetic uses native promotions,
  but / always returns REAL. Cast aliases must precede their use. long double
  may have padding; sizeof does not identify its floating-point format. Native
  C support is verified on Linux LLVM 23; cross-target is unsupported.
- **EXTERN "C" extension.** Top-level, body-less; parameters/results are C types only,
  no BYREF/OVERLOAD/structs. Variadic arguments must be C values (default promotions).
  The driver checks every EXTERN symbol against INCLUDE_OBJ inputs before CodeGen;
  `"__libc"` must be included explicitly even though the runtime always links libc.
  Only `.o` user objects; INCLUDE_OBJ is global across inputs. `-S` does not link them.
  Identical EXTERN redeclarations (same or other files, CIdentity-equal types) are allowed;
  differing ones fail. Two EXTERNs differing only in case conflict (pseudocode names are case insensitive).
  Windows `-r` resolves C symbols from msvcrt.dll before the process-wide search: ucrtbase.dll
  may also be loaded, and its separate stdout buffer reorders EXTERN output against OUTPUT.

## Not implemented (9618 features)

SET types (`SET`/`DEFINE` are reserved with dedicated errors),
random files (OPENFILE...FOR RANDOM/SEEK/GETRECORD/PUTRECORD — TYPE is done, so these
are unblocked now; the RANDOM keyword is reserved and gives a dedicated parse error),
DATE,
array-typed record fields, whole-array RETURN
and class inheritance (INHERITS/SUPER). Compiler fixtures now live in `tests/` (see TESTING.md).
Nightly CI runs the compiler suites, packaged EMBEDDED smoke checks and existing
plugin tests. The initial standard conflicts and missing diagnostic locations are fixed on
Linux.
WHILE accepts both the standard header without DO and the older WHILE DO form.

- **Literal escapes.** C++17 escapes are decoded by the lexer. Unicode escapes encode UTF-8 in strings; CHAR Unicode escapes must be ASCII. Built-in STRING remains NUL-terminated: `"a\0b"` is observed as `"a"`.
