// Both files declare the same C function; aliases of int are interchangeable.
INCLUDE "lib.cps"
INCLUDE_OBJ "__libc"
EXTERN "C" EXTERN_FUNCTION abs(C_INT) RETURNS C_INT
OUTPUT abs((C_INT)-3)
OUTPUT Twice((C_INT)-4)
