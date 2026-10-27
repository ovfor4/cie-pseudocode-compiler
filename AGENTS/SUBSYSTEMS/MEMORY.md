# Explicit memory

Part of the [subsystem notes](../SUBSYSTEMS.md). Related:
[class lifetime](CLASS_LIFETIME.md).

**Explicit memory** lives in `pseudocode-lib/memory.cps` and `memory.cc`.
MALLOC/REALLOC/FREE are ordinary OVERLOAD functions calling `_memory_impl_*`
EXTERN "C" helpers. `lib/CodeGen/MemoryGen.cc` only bridges keyword NEW/DELETE to
visible helper declarations, checking their signatures and using FunctionGen's
C ABI lowering, library call-line hook and exception-aware calls.

## Tracking

- memory.cc owns a linked list of live allocation start addresses. Explicit
  MALLOC/REALLOC and keyword NEW share it; STRING/ARRAY internals keep their own
  ownership outside the list.
- Errors print the existing reason with __cps_user_line and throw 0. The call hook
  supplies the user line from ordinary source positions, including keyword NEW/DELETE helper calls.
- Sizes accept INTEGER and every supported C integer (including _Bool) through
  ordinary overloads. Wrappers explicitly cast to unsigned long long; C++ converts
  that value to native size_t, preserving the old integer conversion behavior.
- std.cps imports memory.cps. With -no-std, explicitly import and supply memory.cps,
  including for keyword NEW/DELETE. Missing visible helpers are compiler errors.

## Operations

- `_memory_impl_validate` always searches, including for null, so keyword DELETE
  validates before running destructors.
- FREE validates non-null addresses, unlinks the tracking node, releases the block
  and node, and returns the original address. Null is returned without searching.
- Zero-byte allocation returns null.
- REALLOC validates non-null addresses before its zero-size release path; null
  behaves as MALLOC. Positive-size resizing updates the node only after success,
  leaving the old block and record intact on failure.
- Node allocation precedes the user allocation; if the latter fails, its node is
  freed before the diagnostic and anonymous throw.
- Keyword NEW reserves at least one byte for method-only classes. NEW constructs
  objects and DELETE destroys them; FREE/REALLOC never invoke destruction.

## Limitation

Address reuse can make stale aliases indistinguishable from current allocations;
dereferences are not checked against this list.
