TYPE Node
    DECLARE Value : INTEGER
ENDTYPE
TYPE Link = ^Node
TYPE Season = (Spring, Summer)
TYPE Signed = C_TYPE("int")
CLASS Box
    Value : INTEGER
    PUBLIC Other : INTEGER
    PRIVATE Hidden : INTEGER
ENDCLASS
CONSTANT Limit = 3
DECLARE Item : nODE
DECLARE Ptr : lINK
DECLARE Mode : sEASON
DECLARE Crate : bOX
DECLARE Items : ARRAY[1:2] OF INTEGER
DECLARE Index : INTEGER
item.value <- limit
pTR <- ^ITEM
ptr^.vALUE <- 7
mode <- sUMMER
crate.value <- 8
CRATE.other <- 9
CALL cHANGE(iTEMS)
FOR Index <- 1 TO 2
    OUTPUT items[index]
NEXT iNDEX
OUTPUT rEADnODE(ITEM), ":", MODE = summer
OUTPUT crate.VALUE, ":", crate.OTHER
OUTPUT length("MiXeD"), ":", (sIGNED)3
IF TRUE THEN
    CONSTANT lIMIT = 5
    OUTPUT LIMIT
ENDIF
OUTPUT Limit
PROCEDURE Change(BYREF Values : ARRAY OF INTEGER)
    vALUES[1] <- 4
    VALUES[2] <- 6
ENDPROCEDURE
FUNCTION ReadNode(Value : NODE) RETURNS INTEGER
    RETURN vALUE.Value
ENDFUNCTION
