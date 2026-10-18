GLOBAL
    DECLARE Live : INTEGER
    DECLARE Copies : INTEGER
ENDGLOBAL
CLASS Traced
    Value : INTEGER
    OVERLOAD PROCEDURE NEW()
        Live <- Live + 1
    ENDPROCEDURE
    OVERLOAD PROCEDURE NEW(BYREF Other : Traced)
        Value <- Other.Value
        Live <- Live + 1
        Copies <- Copies + 1
    ENDPROCEDURE
    PROCEDURE DELETE()
        OUTPUT "destroy:", Value
        Live <- Live - 1
    ENDPROCEDURE
    FUNCTION Read() RETURNS INTEGER
        RETURN Value
    ENDFUNCTION
    FUNCTION Explode() RETURNS INTEGER
        THROW
    ENDFUNCTION
ENDCLASS
FUNCTION Make(N : INTEGER) RETURNS Traced
    DECLARE Local : Traced
    Local.Value <- N
    RETURN Local
ENDFUNCTION
FUNCTION Forward(N : INTEGER) RETURNS Traced
    RETURN Make(N)
ENDFUNCTION
PROCEDURE Pass(Item : Traced)
    OUTPUT "pass:", Item.Value
    Item.Value <- Item.Value + 10
ENDPROCEDURE
FUNCTION Fail() RETURNS INTEGER
    THROW
ENDFUNCTION
PROCEDURE Accept(Item : Traced, Ignored : INTEGER)
ENDPROCEDURE
PROCEDURE Work()
    DECLARE A : Traced
    A.Value <- 1
    CALL Pass(A)
    CALL Pass(Forward(2))
    CALL Make(3)
    DECLARE B : Traced(A)
    DECLARE C : Traced(Make(8))
    OUTPUT Make(4).Read()
    TRY
        OUTPUT Make(6).Explode()
    CATCH
        OUTPUT "caught:", Live
    ENDTRY
    TRY
        CALL Accept(Make(9), Fail())
    CATCH
        OUTPUT "argument caught:", Live
    ENDTRY
    OUTPUT "counts:", Copies, ":", Live
ENDPROCEDURE
CALL Work()
OUTPUT "remaining:", Live
