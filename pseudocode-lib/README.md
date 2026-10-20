# Experimental string library

Build with `cmake --build build`. Include the built library in your program and pass it as a compiler input:

```text
INCLUDE "build/pseudocode-lib/_exp_string.cps"
DECLARE Greeting : _EXP_STRING((C_STRING)"hello")
CALL _EXP_STRING_OUTPUT(Greeting & _EXP_STRING_NUM_TO_STR(42))
```

```sh
build/cpsc prog.cps build/pseudocode-lib/_exp_string.cps
build/cpsc prog.cps build/pseudocode-lib/_exp_string.cps -r
```

INCLUDE paths are relative to the program. Adjust them when the program is elsewhere.

`_EXP_STRING` owns an unbounded `std::string` through one handle. Constructors accept no arguments, another `_EXP_STRING`, `C_STRING`, or `(INTEGER count, C_CHAR character)`. Cast literals to C types explicitly; built-in STRING is not a constructor argument. Copy construction and assignment own independent strings. `_HANDLE` is public only for library implementation access; do not modify or free it.

The library provides `&`, `_EXP_STRING_NUM_TO_STR` (INTEGER/REAL), `_EXP_STRING_STR_TO_NUM` (REAL), `_EXP_STRING_LENGTH` (byte length) and `_EXP_STRING_OUTPUT` (text plus newline). Numeric formatting uses `std::to_string`; parsing uses `std::stod` and requires full consumption. C++ standard exceptions print the user call line and are catchable with TRY/CATCH. The built-in STRING remains available with its existing lifetime and NUL limitations. Owning class values inside whole-record or BYVAL-array copies still have bitwise-copy limitations.

`_EXP_STRING_LEFT(S : _EXP_STRING, Count : INTEGER)` returns a new `_EXP_STRING` containing up to Count bytes from the beginning. Nonpositive counts return an empty string; counts exceeding the length return the whole string. Embedded NUL bytes are preserved.

`_EXP_STRING_RIGHT(S : _EXP_STRING, Count : INTEGER)` returns a new `_EXP_STRING` containing up to Count bytes from the end. Nonpositive counts return an empty string; counts exceeding the length return the whole string. Embedded NUL bytes are preserved.

`_EXP_STRING_MID(S : _EXP_STRING, Start : INTEGER, Count : INTEGER)` returns a new `_EXP_STRING` containing up to Count bytes from the one-based Start position. Starts below 1 clamp to 1; starts beyond the end and nonpositive counts return an empty string. Counts clamp to the remaining bytes. Embedded NUL bytes are preserved.

`_EXP_STRING_LCASE(S : _EXP_STRING)` returns a new `_EXP_STRING` with letters converted to lower case. Its `(Ch : CHAR)` overload returns CHAR, following guide section 5.5. Nonletters and embedded NUL bytes are preserved; the source is unchanged. Use ASCII for predictable case conversion.

`_EXP_STRING_UCASE(S : _EXP_STRING)` returns a new `_EXP_STRING` with letters converted to upper case. Its `(Ch : CHAR)` overload returns CHAR, following guide section 5.5. Nonletters and embedded NUL bytes are preserved; the source is unchanged. Use ASCII for predictable case conversion.

`_EXP_STRING_ASC(S : _EXP_STRING)` returns the unsigned code of the first byte as INTEGER, or 0 for an empty string. Its `(Ch : CHAR)` overload returns the unsigned byte code of Ch. For ASCII characters this is the ASCII code.
