// extern "C" symbols from a C++ object; the C++ names would otherwise be mangled.
INCLUDE_OBJ "lib.o"
INCLUDE_OBJ "__libc"
EXTERN "C" EXTERN_FUNCTION cc_sum() RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION cc_add(C_INT) RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION cc_text_length(C_STRING) RETURNS C_INT
EXTERN "C" EXTERN_PROCEDURE cc_say(C_STRING)
OUTPUT cc_sum()
OUTPUT cc_add((C_INT)5)
OUTPUT cc_add((C_INT)7)
OUTPUT cc_sum()
OUTPUT cc_text_length((C_STRING)"abc")
CALL cc_say((C_STRING)"hi from lib.cc")
