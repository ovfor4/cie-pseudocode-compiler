# Standard library

Part of the [subsystem notes](../SUBSYSTEMS.md). `PP_DEFINE __LIB` library regions and default
import rules: [CONVENTIONS/EXCEPTIONS_AND_UNITS.md](../CONVENTIONS/EXCEPTIONS_AND_UNITS.md).

**Standard library** (`pseudocode-lib/`)

## Build and embedding

- The default build compiles each .cc with the host compiler into
  build/pseudocode-lib/*.o, copies .cps units beside them, and embeds all these outputs
  in cpsc.
- StandardLibrary extracts the resources through linking/execution into a per-user,
  content-keyed cache (`llvm::sys::path::cache_directory()/cpsc/std-<xxh3 hex>`). Files are
  reused when identical, otherwise published by atomic rename; the cache is never deleted, so
  a killed run leaks nothing. Only if no cache directory is usable does it fall back to a
  temporary `cpsc-std` directory removed on exit. The driver
  implicitly imports std.cps unless -no-std is set. std.cps imports string.cps and memory.cps.
- Source/object resources are embedded in installed/released cpsc, which needs no
  separate library directory; manually linking saved IR still requires the matching
  string.o, memory.o and the C++ runtime for default-library builds.

## Explicit memory

- memory.cps defines ordinary OVERLOAD MALLOC/REALLOC/FREE wrappers over memory.cc
  C ABI helpers. Sizes support INTEGER and all supported C integers.
- Keyword NEW/DELETE call visible EXTERN helpers through MemoryGen and FunctionGen;
  no compiler allocation list or memory builtin dispatch remains. With -no-std,
  explicitly import memory.cps, including when only keyword NEW/DELETE is needed.
- Runtime errors print __cps_user_line and throw 0. See [MEMORY.md](MEMORY.md).

## STRING and I/O bridging

- The STRING class is defined under PP_DEFINE __LIB. Its public internal void*
  handle owns std::string. The _string_impl_* C ABI reports __cps_user_line and throws 0 on std::exception.
- Functions/operators use ordinary resolution; StringValueGen bridges literals, C_STR, C
  conversions and file operands.
- C_STRING literals have an adjacent uppercase C suffix.
- STRING INPUT delegates to READ_INPUT (rest of the line, throws at end of input), OUTPUT uses C stdout buffers, and READFILE invokes
  assignment from C_STRING then frees its temporary buffer.
