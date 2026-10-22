// Records cross the EXTERN boundary only as C structs.
INCLUDE_OBJ "__libc"
TYPE Person
    DECLARE Name : STRING
    DECLARE Age : C_INT
ENDTYPE
TYPE PPerson = ^Person
EXTERN "C" EXTERN_PROCEDURE puts(PPerson)
EXTERN "C" EXTERN_FUNCTION abs(Person) RETURNS C_INT
OUTPUT 1
