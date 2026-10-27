# Conventions: STRING values and ownership

Part of [Load-bearing conventions](../CONVENTIONS.md).

**STRING values use the class machinery.** `string.cps` owns a std::string through an
internal handle. Ordinary literals construct STRING through NEW(C_STRING); suffixed
`"text"C` literals produce C_STRING directly. Ordinary literals retain legacy NUL
truncation. `StringValueGen.cc` bridges construction, C_STR access and file operands;
INPUT calls READ_INPUT. String functions and all concatenation/comparison operators
are ordinary library definitions, with exact overload selection. Comparisons use
case-sensitive byte ordering. A missing STRING class or required bridge method is a
compile error.

**CHAR and STRING convert implicitly.** `emitCoercedExpr` turns a CHAR into a
one-character STRING (`emitStringFromChar`, STRING.NEW(1, ch)) and a STRING into a CHAR
(`emitCharFromString`, via the `_LENGTH()` and `C_STR()` bridge methods), so assignment,
parameter passing and RETURN need no special cases; a length other than 1 is a runtime
error. `resolveOperator` and `resolveCall` fall back to the STRING overload when an
exact match is missing and a CHAR is involved (`'a' & 'b'`, `"a" = 'a'`); two CHAR
operands still compare as CHARs, and CASE converts the CHAR side itself. A converted
value is a fresh temporary, so `emitAssignment` copies its source (CopySource) only when
the source type equals the target type. CHAR with INTEGER or BOOLEAN never converts.

**Ownership follows value copies.** STRING constructors/assignment prevent shared
handles. Records and BYVAL arrays containing STRING recursively copy their
string-bearing fields/elements; RETURN and temporary cleanup preserve ownership.
C_STRING casts borrow a STRING buffer or construct an owning STRING copy in the other
direction. FileHandler owns copied open-file names and frees them on close; READFILE
buffers are copied into STRING and released. Explicit memory-library allocations remain
separate from these internal objects.
