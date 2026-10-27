# Standard pseudocode library

`cpsc` embeds `std.cps`, its imported `string.cps` and `memory.cps`, and the matching `string.o` and `memory.o`. Programs use STRING, its functions and the memory functions without INCLUDE statements or extra command-line inputs:

```text
DECLARE Greeting : STRING
Greeting <- "hello"
OUTPUT Greeting & NUM_TO_STR(42)
```

Use `-no-std` to disable automatic loading and linking. For an explicit library build, include the built unit from your program and pass every imported unit as a compiler input:

```text
INCLUDE "build/pseudocode-lib/std.cps"
OUTPUT LENGTH("hello")
```

```sh
cmake --build build
build/cpsc -no-std prog.cps build/pseudocode-lib/std.cps build/pseudocode-lib/string.cps build/pseudocode-lib/memory.cps -r
```

INCLUDE paths are relative to the including source. Adjust the example paths when the program is elsewhere. Native compilation and `-r` automatically link the required library objects. `-S` only emits IR; manually linking default-library IR requires `build/pseudocode-lib/string.o`, `build/pseudocode-lib/memory.o` and a compatible C++ runtime.

`memory.cps` provides ordinary `MALLOC(bytes)`, `REALLOC(pointer, bytes)` and `FREE(pointer)` functions. Sizes accept INTEGER or any supported C integer, including _Bool, and convert to native size_t; pointer arguments and all results use C `void*`. Zero-byte allocation returns null. FREE(null) returns null; other FREE calls return the released address. REALLOC(null, size) allocates, and REALLOC(pointer, 0) validates and frees the allocation, returning null. Failed nonzero resizing leaves the original block valid. Invalid addresses and allocation failures print the user call line and throw an anonymous exception.

These functions and keyword NEW/DELETE share one allocation list. NEW constructs objects; DELETE validates before destruction; FREE and REALLOC perform no destruction. STRING/ARRAY internal buffers are outside this list. Addresses must start active allocations; tracking cannot distinguish stale aliases after address reuse or validate memory accesses.

With `-no-std`, keyword NEW/DELETE also require the memory library. A program can import only memory.cps, without STRING:

```text
INCLUDE "build/pseudocode-lib/memory.cps"
TYPE Raw = C_TYPE("void*")
DECLARE P : Raw
P <- MALLOC(8)
FREE(P)
```

```sh
build/cpsc -no-std prog.cps build/pseudocode-lib/memory.cps -r
```

Manual linking of IR from this memory-only build requires memory.o and the C++ runtime.

STRING is an owning class backed by `std::string`. Constructors accept no arguments, another STRING, C_STRING, or `(INTEGER count, C_CHAR character)`. Copies and assignments are independent, including STRING fields inside records and BYVAL arrays. `_HANDLE` is public for implementation access; do not modify or free it.

Ordinary double-quoted literals construct STRING through `NEW(C_STRING)` and require a visible STRING class. They retain the legacy NUL truncation: `"a\0b"` constructs `"a"`. The adjacent uppercase suffix `"hello"C` creates C_STRING directly and works with `-no-std`. Explicit casts convert C_STRING to an owning STRING, treating null as empty. `(C_STRING)Text` and `Text.C_STR()` borrow a NUL-terminated pointer valid until Text changes or is destroyed. INPUT reads an unbounded whitespace-delimited word; OUTPUT and file I/O use NUL-terminated text.

The library provides `&`, six case-sensitive content comparisons, and these ordinary functions:

- `LENGTH(S)` returns the byte length, including embedded NUL bytes constructed through CHR or the count/character constructor.
- `LEFT(S, Count)` and `RIGHT(S, Count)` return up to Count bytes. Nonpositive counts return empty; oversized counts return the whole string.
- `MID(S, Start, Count)` uses a one-based Start. Starts below 1 clamp to 1; starts beyond the end and nonpositive counts return empty. Counts clamp to the remaining bytes.
- `LCASE(CHAR)` and `UCASE(CHAR)` return CHAR, following guide section 5.5. STRING overloads return independent strings. Nonletters and embedded NUL bytes are preserved; use ASCII for predictable case conversion.
- `ASC(CHAR)` and `ASC(STRING)` return the unsigned character or first-byte code; an empty string returns zero.
- `CHR(Code)` returns a CHAR containing the low eight bits. `LENGTH(CHR(0))` is 1.
- `NUM_TO_STR(INTEGER)` and `NUM_TO_STR(REAL)` convert INTEGER to decimal text and REAL to the shortest round-trip text (Python `repr` style, identical to OUTPUT: `2.0`, `0.1`, `1e-07`, `inf`).
- `STR_TO_NUM(S)` accepts STRING or CHAR and returns REAL through the same `parseNumber` helper as IS_NUM (`strtod`, full consumption, decimal text only: nan, inf/infinity and hexadecimal are rejected). Invalid text and trailing junk throw `STR_TO_NUM: '<text>' is not a number`; overflow throws `... is out of the REAL range`; underflow is accepted.
- `IS_NUM(S)` agrees with STR_TO_NUM: full consumption by `strtod` and no overflow, without throwing. Leading whitespace is accepted; nan, inf/infinity, hexadecimal, trailing whitespace, trailing junk, empty input, embedded NUL bytes, and overflowing values (`1e999`) are rejected.

Library C++ exceptions print the user call line and can be caught with TRY/CATCH. Library functions follow the guide where specified; LEFT, STRING case conversion, byte-code and numeric-conversion functions remain compatibility extensions.
