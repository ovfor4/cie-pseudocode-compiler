# Conventions: STRING values and ownership

Part of [Load-bearing conventions](../CONVENTIONS.md).

**Literals are raw C_STRING values.** Both ordinary and C-suffixed double-quoted
literals emit global text pointers; CONSTANT keeps the same raw type and read-only
binding. Text operations retain legacy NUL truncation. Single quotes remain CHAR.
An exact C_STRING overload wins over conversion to STRING.

**Automatic construction is opt-in.** `string.cps` owns a std::string through an
internal handle and declares IMPLICIT NEW(C_STRING) and IMPLICIT NEW(CHAR).
`TypeCheckGen.cc` shares direct constructor lookup, conversion ranking and emission
with calls, operators and captured CASE values. Never automatically invoke an
unmarked constructor, including in a replacement STRING class under -no-std.
Explicit casts may still use unmarked NEW(C_STRING).

**CHAR compatibility remains.** STRING -> CHAR uses `_LENGTH()` and `C_STR()` and
checks length 1 at runtime. C_STRING -> CHAR first uses the marked STRING constructor;
this compatibility bridge ranks below a direct STRING parameter, preserving string
function selection. Two CHARs compare directly. No path may chain two IMPLICIT NEWs.

**I/O bridges retain STRING behavior.** OUTPUT and file operands convert C_STRING
through its marked STRING constructor, then use C_STR. INPUT calls READ_INPUT.
String functions, concatenation and comparisons are ordinary library definitions;
comparisons use case-sensitive byte ordering. Missing classes or methods diagnose.

**Ownership follows value copies.** STRING constructors/assignment prevent shared
handles. Records and BYVAL arrays containing STRING recursively clone owned string
fields/elements. Converted arguments stay Alive until ownership transfers to the
callee; failed later arguments release pending values. Converted lvalues are fresh
objects, never copy sources of a different type. RETURN, discarded values and
statement temporaries use ordinary class cleanup, including exception exits.
Explicit C_STRING casts borrow a STRING buffer or construct an owning copy in the
other direction. FileHandler copies open-file names and releases them on close;
READFILE copies and releases its buffer. Explicit memory allocations remain separate.
