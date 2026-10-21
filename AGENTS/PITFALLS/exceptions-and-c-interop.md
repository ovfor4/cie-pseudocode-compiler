# Exceptions, C types and EXTERN "C"

Part of the [pitfalls index](../PITFALLS.md).

## Exception extensions

- TRY requires exactly one parameterless CATCH and ENDTRY.
- Bare THROW raises a new anonymous exception even inside CATCH; there is no rethrow
  syntax, error object or what(). Bare THROW prints no diagnostic itself.
- Runtime checks are catchable; compiler errors are not. Runtime checks print the
  source line and error reason to stderr before throwing, even when caught.
- Uncaught exceptions also print the generic termination message and return 1.
- Native binaries must link the C++ runtime with LLVM 23's `clang++` (`clang++-23` on
  Debian/Ubuntu).
- Owned STRING values follow class cleanup during unwinding; raw array buffers are
  not freed and files are not automatically closed.
- Embedded ORC execution with LLVM 23 is exercised by nightly CI on Linux, Windows
  and macOS; check the native job results when changing platform-specific code.

## C type extensions

- C_STRING is a raw char pointer, not a STRING: explicitly cast to STRING for
  printing or string-library calls.
- C aliases share canonical CIdentity; do not use LLVM widths to recover signedness
  or nominal type identity. Plain char's signedness is native, unlike pseudocode CHAR.
- C/pseudocode conversions require casts even for literals. C scalar arithmetic uses
  native promotions, but / always returns REAL.
- Cast aliases must precede their use.
- long double may have padding; sizeof does not identify its floating-point format.
- Native C support is verified on Linux LLVM 23; cross-target is unsupported.

## EXTERN "C" extension

- Top-level, body-less; parameters/results are C types only, no BYREF/OVERLOAD/structs.
  Variadic arguments must be C values (default promotions).
- The driver checks every EXTERN symbol against INCLUDE_OBJ inputs before CodeGen;
  `"__libc"` must be included explicitly even though the runtime always links libc.
- Only `.o` user objects; INCLUDE_OBJ is global across inputs. `-S` does not link them.
- Identical EXTERN redeclarations (same or other files, CIdentity-equal types) are
  allowed; differing ones fail. Two EXTERNs differing only in case conflict
  (pseudocode names are case insensitive).
- Windows `-r` resolves C symbols from msvcrt.dll before the process-wide search:
  ucrtbase.dll may also be loaded, and its separate stdout buffer reorders EXTERN
  output against OUTPUT.
