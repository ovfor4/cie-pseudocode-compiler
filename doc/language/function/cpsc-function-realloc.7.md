% CPSC-FUNCTION-REALLOC(7)

# NAME

cpsc-function-realloc - resize an explicit memory allocation

# SYNOPSIS

```text
REALLOC(pointer, bytes)
```

# DESCRIPTION

REALLOC is a compiler extension returning C `void*`. The pointer argument must also be C `void*`; explicitly cast other pointer types. The size accepts INTEGER or a C integer and converts to host size_t, just as MALLOC does.

A null pointer behaves as MALLOC(bytes). A non-null pointer must start an active tracked MALLOC, REALLOC or keyword NEW allocation, even when bytes is zero. Invalid addresses print the source line and reason to stderr and throw an anonymous exception before touching the memory or allocation record. Resizing or releasing memory with REALLOC does not invoke constructors or destructors.

For a valid pointer and zero bytes, REALLOC releases the block and returns null. This rule is independent of libc's zero-size behavior. For a nonzero size, successful resizing preserves the smaller of the old and new sizes; added bytes are uninitialized. The returned address may differ from the input. Use the returned pointer afterward; previous aliases must not be used.

On nonzero allocation failure, REALLOC prints the source line and reason and throws an anonymous exception. The original allocation and its tracking record remain valid. In `p <- REALLOC(p, bytes)`, assignment occurs only on success, so a CATCH handler can still use or release the old p. A successful resize updates the existing record without allocating a new tracking node. Internal STRING/ARRAY buffers cannot be resized with this function.

# EXAMPLE

```text
TYPE Raw = C_TYPE("void*")
TYPE IntPointer = ^INTEGER
DECLARE Memory : Raw
DECLARE Pointer : IntPointer
Memory <- MALLOC(8)
Pointer <- (IntPointer)Memory
Pointer^ <- 42
Memory <- REALLOC(Memory, 16)
Pointer <- (IntPointer)Memory
OUTPUT Pointer^
FREE(Memory)
```

# SEE ALSO

[cpsc-function-malloc(7)](cpsc-function-malloc.7.md), [cpsc-function-free(7)](cpsc-function-free.7.md), [cpsc-keyword-try(7)](../keyword/cpsc-keyword-try.7.md).
