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

**Ownership follows value copies.** STRING constructors/assignment prevent shared
handles. Records and BYVAL arrays containing STRING recursively copy their
string-bearing fields/elements; RETURN and temporary cleanup preserve ownership.
C_STRING casts borrow a STRING buffer or construct an owning STRING copy in the other
direction. FileHandler owns copied open-file names and frees them on close; READFILE
buffers are copied into STRING and released. Explicit MemoryHandler allocations remain
separate from these internal objects.
