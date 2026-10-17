CLASS Trace
    Id : INTEGER
    PROCEDURE DELETE()
        OUTPUT "destroy:", Id
    ENDPROCEDURE
ENDCLASS
CLASS Bomb
    PROCEDURE DELETE()
        OUTPUT "bomb"
        THROW
    ENDPROCEDURE
ENDCLASS
PROCEDURE Normal()
    DECLARE First : Trace
    First.Id <- 1
    DECLARE Last : Bomb
ENDPROCEDURE
PROCEDURE DuringUnwind()
    DECLARE Last : Bomb
    THROW
ENDPROCEDURE
PROCEDURE InCatch()
    DECLARE First : Trace
    First.Id <- 2
    TRY
        THROW
    CATCH
        DECLARE Last : Bomb
        RETURN
    ENDTRY
ENDPROCEDURE
PROCEDURE InTry()
    DECLARE Last : Bomb
    TRY
        RETURN
    CATCH
        OUTPUT "return caught locally"
    ENDTRY
ENDPROCEDURE
FUNCTION Result() RETURNS INTEGER
    DECLARE Local : Trace
    Local.Id <- 3
    RETURN Local.Id
ENDFUNCTION
TRY
    CALL Normal()
CATCH
    OUTPUT "normal caught"
ENDTRY
TRY
    CALL InCatch()
CATCH
    OUTPUT "catch exit caught"
ENDTRY
CALL InTry()
OUTPUT Result()
TRY
    CALL DuringUnwind()
CATCH
    OUTPUT "unwind throw caught"
ENDTRY
