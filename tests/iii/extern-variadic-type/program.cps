// Variadic arguments must be C values.
INCLUDE_OBJ "__libc"
EXTERN "C" EXTERN_FUNCTION printf(C_STRING, ...) RETURNS C_INT
DECLARE N : C_INT
N <- printf((C_STRING)"%d", 1)
