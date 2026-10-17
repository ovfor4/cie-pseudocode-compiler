# Trace notes

Human review: pending.

Explicit memory allocation is a compiler extension. Zero size and FREE(null) return null. An INTEGER allocation stores 42; a two-INTEGER record stores 11 and 22. FREE returns the input address. Freeing the middle, head and tail nodes empties the allocation list. Repeated free, a stack address and a record field's interior address are caught before libc is called. The negative size converts to SIZE_MAX, whose allocation fails. A final allocation still stores 99. Sizes 8 and 16 cover the fixed i64 representation.
