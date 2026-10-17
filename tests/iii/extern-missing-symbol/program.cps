// putz is not defined by the C library.
INCLUDE_OBJ "__libc"
EXTERN "C" EXTERN_FUNCTION putz(C_STRING) RETURNS C_INT
OUTPUT 1
