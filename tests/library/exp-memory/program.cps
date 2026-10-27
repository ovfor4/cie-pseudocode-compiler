INCLUDE "../../../pseudocode-lib/memory.cps"
TYPE Raw = C_TYPE("void*")
DECLARE Empty : Raw
TYPE Size0 = C_TYPE("_Bool")
TYPE Size1 = C_TYPE("char")
TYPE Size2 = C_TYPE("signed char")
TYPE Size3 = C_TYPE("unsigned char")
TYPE Size4 = C_TYPE("short")
TYPE Size5 = C_TYPE("unsigned short")
TYPE Size6 = C_TYPE("int")
TYPE Size7 = C_TYPE("unsigned int")
TYPE Size8 = C_TYPE("long")
TYPE Size9 = C_TYPE("unsigned long")
TYPE Size10 = C_TYPE("long long")
TYPE Size11 = C_TYPE("unsigned long long")
OUTPUT malloc((Size0)0) = Empty, realloc(Empty, (Size0)0) = Empty
FREE(REALLOC(MALLOC((Size0)1), (Size0)2))
OUTPUT malloc((Size1)0) = Empty, realloc(Empty, (Size1)0) = Empty
FREE(REALLOC(MALLOC((Size1)1), (Size1)2))
OUTPUT malloc((Size2)0) = Empty, realloc(Empty, (Size2)0) = Empty
FREE(REALLOC(MALLOC((Size2)1), (Size2)2))
OUTPUT malloc((Size3)0) = Empty, realloc(Empty, (Size3)0) = Empty
FREE(REALLOC(MALLOC((Size3)1), (Size3)2))
OUTPUT malloc((Size4)0) = Empty, realloc(Empty, (Size4)0) = Empty
FREE(REALLOC(MALLOC((Size4)1), (Size4)2))
OUTPUT malloc((Size5)0) = Empty, realloc(Empty, (Size5)0) = Empty
FREE(REALLOC(MALLOC((Size5)1), (Size5)2))
OUTPUT malloc((Size6)0) = Empty, realloc(Empty, (Size6)0) = Empty
FREE(REALLOC(MALLOC((Size6)1), (Size6)2))
OUTPUT malloc((Size7)0) = Empty, realloc(Empty, (Size7)0) = Empty
FREE(REALLOC(MALLOC((Size7)1), (Size7)2))
OUTPUT malloc((Size8)0) = Empty, realloc(Empty, (Size8)0) = Empty
FREE(REALLOC(MALLOC((Size8)1), (Size8)2))
OUTPUT malloc((Size9)0) = Empty, realloc(Empty, (Size9)0) = Empty
FREE(REALLOC(MALLOC((Size9)1), (Size9)2))
OUTPUT malloc((Size10)0) = Empty, realloc(Empty, (Size10)0) = Empty
FREE(REALLOC(MALLOC((Size10)1), (Size10)2))
OUTPUT malloc((Size11)0) = Empty, realloc(Empty, (Size11)0) = Empty
FREE(REALLOC(MALLOC((Size11)1), (Size11)2))
CLASS Item
    Value : INTEGER
ENDCLASS
TYPE ItemPointer = ^Item
DECLARE Obj : Item
DECLARE P : ItemPointer
DECLARE Address : Raw
Obj <- NEW Item()
Obj.Value <- 7
P <- ^Obj
Address <- REALLOC((Raw)P, 16)
P <- (ItemPointer)Address
OUTPUT P^.Value
DELETE P^
P <- (ItemPointer)MALLOC(8)
P^.Value <- 17
OUTPUT P^.Value
DELETE P^
Obj <- NEW Item()
P <- ^Obj
Address <- (Raw)P
FREE(Address)
TRY
    DELETE Obj
CATCH
    OUTPUT 1
ENDTRY
TRY
    MALLOC((Size2)-1)
CATCH
    OUTPUT 2
ENDTRY
TRY
    MALLOC((Size11)-1)
CATCH
    OUTPUT 3
ENDTRY
