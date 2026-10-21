# Explicit memory

Part of the [subsystem notes](../SUBSYSTEMS.md). Related:
[class lifetime](CLASS_LIFETIME.md).

**Explicit memory** (`MemoryHandler`): lazily emits internal allocation/find/release/
resize helpers and a linked list of live allocation start addresses.

## Tracking

- Explicit MALLOC/REALLOC and keyword NEW allocations enter the list; STRING/ARRAY
  internals are not tracked here and keep their own ownership.
- Helpers use RuntimeCheck and ExceptionHandler, including source-line arguments and
  per-function unwind preparation.
- Sizes accept INTEGER and C integers and convert to native size_t.

## Operations

- `emitValidate` always calls the find helper, including for null, so keyword DELETE
  validates before running destructors.
- FREE validates non-null addresses, unlinks and frees the tracking node, releases the
  block and returns the original address.
- Zero-byte allocation returns null.
- REALLOC validates non-null addresses before its zero-size release path; null behaves
  as MALLOC. Positive-size resizing updates the node only after success, leaving the old
  block and record intact on failure.
- Node allocation precedes the user allocation; if the latter fails, its node is freed
  before the diagnostic and anonymous throw.

## Limitation

Address reuse can make stale aliases indistinguishable from current allocations;
dereferences are not checked against this list.
