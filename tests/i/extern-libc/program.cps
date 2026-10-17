// C library calls through EXTERN, including variadic promotions.
INCLUDE_OBJ "__libc"
TYPE CFloat = C_TYPE("float")
TYPE CShort = C_TYPE("short")
EXTERN "C" EXTERN_FUNCTION printf(C_STRING, ...) RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION puts(C_STRING) RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION abs(C_INT) RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION sqrt(C_DOUBLE) RETURNS C_DOUBLE
TYPE CUnsigned = C_TYPE("unsigned int")
EXTERN "C" EXTERN_PROCEDURE srand(CUnsigned)
CALL srand((CUnsigned)1)
DECLARE Count : C_INT
CALL puts((C_STRING)"hello")
OUTPUT abs((C_INT)-5)
OUTPUT sqrt((C_DOUBLE)16)
Count <- printf((C_STRING)"%d %s %.1f %d|", (C_INT)42, (C_STRING)"x", (CFloat)1.5, (CShort)-7)
OUTPUT ""
OUTPUT Count
