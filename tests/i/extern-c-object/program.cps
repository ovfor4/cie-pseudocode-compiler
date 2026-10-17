// Functions from a user C object built with the host C compiler.
INCLUDE_OBJ "lib.o"
EXTERN "C" EXTERN_FUNCTION add3(C_INT, C_INT, C_INT) RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION scale(C_DOUBLE, C_INT) RETURNS C_DOUBLE
EXTERN "C" EXTERN_FUNCTION name_length(C_STRING) RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION next_call() RETURNS C_INT
EXTERN "C" EXTERN_PROCEDURE say(C_STRING)
OUTPUT add3((C_INT)1, (C_INT)2, (C_INT)3)
OUTPUT scale((C_DOUBLE)2.5, (C_INT)4)
OUTPUT name_length((C_STRING)"cambridge")
OUTPUT next_call()
OUTPUT next_call()
CALL say((C_STRING)"from lib.c")
