# Trace notes

Human review: pending.

This case exercises explicit memory extensions. REALLOC(null, 0) returns null; REALLOC(null, 16) allocates. The record's 12 and 34 survive growth, and 12 survives shrinking to one INTEGER. A SIZE_MAX request fails without changing the pointer or its stored 12; a subsequent write of 56 survives resizing. A zero resize frees the block and returns null. Resizing its released address throws; a stack pointer throws even at zero size. C int 71 survives reallocation after an allocation returned from a function. Invalid resizing inside a function unwinds to its caller; another invalid free is caught inside that catch. A STRING literal is rejected as untracked storage. The final nested allocation/resize/free has no output.
