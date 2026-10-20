INCLUDE_OBJ "__libc"
EXTERN "C" EXTERN_FUNCTION puts(C_STRING) RETURNS C_INT
CALL puts("C literal without the standard library"C)
OUTPUT 20 + 22
