// Records cross the EXTERN boundary only as C structs through a pointer type.
INCLUDE_OBJ "__libc"
TYPE Person
    DECLARE Name : STRING
    DECLARE Age : C_INT
ENDTYPE
TYPE PPerson = ^Person
TYPE Point
    DECLARE X : C_INT
    DECLARE Y : C_INT
ENDTYPE
EXTERN "C" EXTERN_PROCEDURE puts(PPerson)
EXTERN "C" EXTERN_FUNCTION abs(Point) RETURNS C_INT
OUTPUT 1
