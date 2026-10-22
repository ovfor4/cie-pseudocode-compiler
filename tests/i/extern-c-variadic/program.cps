// Variadic EXTERN arguments arrive with C default promotions applied.
INCLUDE_OBJ "lib.o"
TYPE UChar = C_TYPE("unsigned char")
TYPE CShort = C_TYPE("short")
TYPE UShort = C_TYPE("unsigned short")
TYPE CBool = C_TYPE("_Bool")
TYPE CFloat = C_TYPE("float")
EXTERN "C" EXTERN_FUNCTION sum_ints(C_INT, ...) RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION sum_doubles(C_INT, ...) RETURNS C_DOUBLE
OUTPUT sum_ints((C_INT)5, (UChar)200, (CShort)-7, (UShort)65535, (CBool)TRUE, (C_CHAR)'A')
OUTPUT sum_doubles((C_INT)2, (CFloat)1.5, (C_DOUBLE)2.25)
