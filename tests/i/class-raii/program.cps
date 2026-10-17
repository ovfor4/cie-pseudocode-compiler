CLASS Traced
    Id : INTEGER
    PROCEDURE NEW()
        Id <- 0
    ENDPROCEDURE
    PROCEDURE DELETE()
        OUTPUT "destroy:", Id
    ENDPROCEDURE
ENDCLASS
TYPE Wrapped
    DECLARE Item : Traced
ENDTYPE
PROCEDURE Copy(Value : Traced)
    DECLARE Local : Traced
    Local.Id <- 20
    Value.Id <- 21
ENDPROCEDURE
PROCEDURE Work()
    DECLARE First : Traced
    First.Id <- 1
    DECLARE Rebound : Traced
    Rebound.Id <- 2
    Rebound <- NEW Traced()
    Rebound.Id <- 99
    DECLARE I : INTEGER
    FOR I <- 1 TO 3
        DECLARE Again : Traced
    Again.Id <- 3
        Again.Id <- I + 30
    NEXT I
    DECLARE Arr : ARRAY[1:2] OF Plain
    Arr[1].Item.Id <- 41
    Arr[2].Item.Id <- 42
    DECLARE Rec : Wrapped
    Rec.Item.Id <- 50
    CALL Copy(First)
    IF TRUE THEN
        RETURN
    ENDIF
    DECLARE Never : Traced
    Never.Id <- 77
ENDPROCEDURE
CLASS Plain
    Item : Traced
ENDCLASS
DECLARE Global : Traced
    Global.Id <- 100
CALL Work()
OUTPUT "main end"
