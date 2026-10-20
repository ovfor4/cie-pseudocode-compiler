# Standard pseudocode library

`cpsc` embeds `std.cps`, its imported `string.cps`, and the matching `string.o`. Programs use STRING and its functions without INCLUDE statements or extra command-line inputs:

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
build/cpsc -no-std prog.cps build/pseudocode-lib/std.cps build/pseudocode-lib/string.cps -r
```

INCLUDE paths are relative to the including source. Adjust the example paths when the program is elsewhere. Native compilation and `-r` automatically link the required library object. `-S` only emits IR; manually linking that IR requires `build/pseudocode-lib/string.o` and a compatible C++ runtime.

STRING is an owning class backed by `std::string`. Constructors accept no arguments, another STRING, C_STRING, or `(INTEGER count, C_CHAR character)`. Copies and assignments are independent, including STRING fields inside records and BYVAL arrays. `_HANDLE` is public for implementation access; do not modify or free it.

Ordinary double-quoted literals construct STRING through `NEW(C_STRING)` and require a visible STRING class. They retain the legacy NUL truncation: `"a\0b"` constructs `"a"`. The adjacent uppercase suffix `"hello"C` creates C_STRING directly and works with `-no-std`. Explicit casts convert C_STRING to an owning STRING, treating null as empty. `(C_STRING)Text` and `Text.C_STR()` borrow a NUL-terminated pointer valid until Text changes or is destroyed. INPUT reads an unbounded whitespace-delimited word; OUTPUT and file I/O use NUL-terminated text.

The library provides `&`, six case-sensitive content comparisons, and these ordinary functions:

- `LENGTH(S)` returns the byte length, including embedded NUL bytes constructed through CHR or the count/character constructor.
- `LEFT(S, Count)` and `RIGHT(S, Count)` return up to Count bytes. Nonpositive counts return empty; oversized counts return the whole string.
- `MID(S, Start, Count)` uses a one-based Start. Starts below 1 clamp to 1; starts beyond the end and nonpositive counts return empty. Counts clamp to the remaining bytes.
- `LCASE(CHAR)` and `UCASE(CHAR)` return CHAR, following guide section 5.5. STRING overloads return independent strings. Nonletters and embedded NUL bytes are preserved; use ASCII for predictable case conversion.
- `ASC(CHAR)` and `ASC(STRING)` return the unsigned character or first-byte code; an empty string returns zero.
- `CHR(Code)` returns a one-byte STRING containing the low eight bits. `LENGTH(CHR(0))` is 1.
- `NUM_TO_STR(INTEGER)` and `NUM_TO_STR(REAL)` use C++17 `std::to_string`, with six fractional digits for REAL.
- `STR_TO_NUM(S)` returns REAL using `std::stod` and requires full consumption. Invalid text, trailing junk, and out-of-range values throw.
- `IS_NUM(S)` tests full consumption by `strtod` without throwing for invalid text. Leading whitespace is accepted; trailing whitespace, trailing junk, empty input, and embedded NUL bytes are rejected.

Library C++ exceptions print the user call line and can be caught with TRY/CATCH. Library functions follow the guide where specified; LEFT, STRING case conversion, byte-code and numeric-conversion functions remain compatibility extensions.
