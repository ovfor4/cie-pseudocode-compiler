INCLUDE_OBJ "runtime.o"
EXTERN "C" EXTERN_FUNCTION string_length() RETURNS C_INT
EXTERN "C" EXTERN_PROCEDURE string_throw()
OUTPUT (INTEGER)string_length()
TRY
    CALL string_throw()
CATCH
    OUTPUT "caught C++"
ENDTRY
