% CPSC-FUNCTION-MALLOC(7)

# NAME

cpsc-function-malloc - allocate uninitialized raw memory

# SYNOPSIS

```text
MALLOC(bytes)
```

# DESCRIPTION

MALLOC is a compiler extension returning a C `void*` to the beginning of an uninitialized allocation. The size is in bytes and accepts INTEGER or a C integer, converted to the host size_t using C integer conversion rules. Negative values are not separately rejected. MALLOC(0) returns null without allocating. A nonzero allocation failure prints the source line and reason to stderr, then throws an anonymous exception catchable with TRY/CATCH. Allocation tracking failures behave the same way.

Declare a `void*` alias with C_TYPE and explicitly cast the result to a C or pseudocode pointer before dereferencing. MALLOC does not initialize fields or call constructors. Keyword NEW uses the same tracked allocation path, then initializes and constructs the class object. The caller of MALLOC must supply enough bytes and initialize values before reading them. User memory is not automatically released.

Explicit MALLOC/REALLOC and keyword NEW allocations are tracked. Internal STRING and ARRAY buffers remain outside this ownership system. Tracking validates FREE, REALLOC and keyword DELETE addresses, not memory accesses, bounds or stale aliases after address reuse.

# EXAMPLE

```text
TYPE Raw = C_TYPE("void*")
TYPE IntPointer = ^INTEGER
DECLARE Memory : Raw
DECLARE Pointer : IntPointer
Memory <- MALLOC(8)
Pointer <- (IntPointer)Memory
Pointer^ <- 42
OUTPUT Pointer^
FREE(Memory)
```

# SEE ALSO

[cpsc-function-free(7)](cpsc-function-free.7.md), [cpsc-function-realloc(7)](cpsc-function-realloc.7.md), [cpsc-keyword-c_type(7)](../keyword/cpsc-keyword-c_type.7.md), [cpsc-keyword-try(7)](../keyword/cpsc-keyword-try.7.md).
