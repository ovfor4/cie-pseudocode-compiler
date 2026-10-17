% CPSC-FUNCTION-FREE(7)

# NAME

cpsc-function-free - release an explicit memory allocation

# SYNOPSIS

```text
FREE(pointer)
```

# DESCRIPTION

FREE is a compiler extension. Its argument and return type are C `void*`. Explicitly cast other pointer types to a `void*` alias before passing them. A null pointer is accepted and returned. A non-null pointer must be the start of an active explicit MALLOC, REALLOC or keyword NEW allocation; internal STRING/ARRAY storage, stack addresses, interior addresses and already released allocations are rejected with a source-line diagnostic on stdout and an anonymous exception.

FREE returns its input address after releasing the allocation. It does not clear the caller's variable. The returned address and all aliases are invalid for memory access. Calling FREE again on that address throws unless a subsequent allocation has reused the address. Tracking cannot distinguish stale aliases once an address is reused.

The result can be discarded with a standalone call. No user destructor is called. Use keyword DELETE on a class object to destroy it and its fields before releasing its allocation.

# EXAMPLE

```text
TYPE Raw = C_TYPE("void*")
DECLARE Memory : Raw
Memory <- MALLOC(8)
FREE(Memory)
```

# SEE ALSO

[cpsc-keyword-delete(7)](../keyword/cpsc-keyword-delete.7.md), [cpsc-function-malloc(7)](cpsc-function-malloc.7.md), [cpsc-function-realloc(7)](cpsc-function-realloc.7.md), [cpsc-keyword-function(7)](../keyword/cpsc-keyword-function.7.md), [cpsc-keyword-try(7)](../keyword/cpsc-keyword-try.7.md).
