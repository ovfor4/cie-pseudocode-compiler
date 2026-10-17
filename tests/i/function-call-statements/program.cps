TYPE Raw = C_TYPE("void*")
TYPE NativePointer = C_TYPE("int*")
TYPE IntPointer = ^INTEGER
DECLARE Number : INTEGER
DECLARE Pointer : IntPointer
DECLARE Address : Raw
DECLARE Native : C_INT
DECLARE NativeAddress : NativePointer
Number <- 17
Pointer <- ^Number
Address <- (Raw)Pointer
Pointer <- (IntPointer)Address
OUTPUT Pointer^
Native <- (C_INT)23
NativeAddress <- ^Native
Address <- (Raw)NativeAddress
NativeAddress <- (NativePointer)Address
OUTPUT NativeAddress^
FUNCTION Echo(Value : INTEGER) RETURNS INTEGER
    OUTPUT Value
    RETURN Value
ENDFUNCTION
Echo(7)
LENGTH("discard")
CALL LENGTH("also discard")
PROCEDURE Change(BYREF Value : INTEGER)
    Value <- Value + 1
ENDPROCEDURE
Change(Number)
CALL Change(Number)
OUTPUT Number
TRY
    Echo(1 DIV 0)
CATCH
    OUTPUT "caught"
ENDTRY
