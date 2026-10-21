# Standard library

Part of the [subsystem notes](../SUBSYSTEMS.md).

**Standard library** (`pseudocode-lib/`)

## Build and embedding

- The default build compiles each .cc with the host compiler into
  build/pseudocode-lib/*.o, copies .cps units beside them, and embeds all these outputs
  in cpsc.
- StandardLibrary extracts the resources through linking/execution; the driver
  implicitly imports std.cps unless -no-std is set. std.cps imports string.cps.
- Source/object resources are embedded in installed/released cpsc; saved IR requires the
  object when manually linked.

## STRING and I/O bridging

- STRING is a #LIB class with a public internal void* handle owning std::string. The
  _string_impl_* C ABI reports __cps_user_line and throws 0 on std::exception.
- Functions/operators use ordinary resolution; StringValueGen bridges literals, C_STR, C
  conversions and file operands.
- C_STRING literals have an adjacent uppercase C suffix.
- INPUT delegates to READ_INPUT, OUTPUT uses C stdout buffers, and READFILE invokes
  assignment from C_STRING then frees its temporary buffer.
