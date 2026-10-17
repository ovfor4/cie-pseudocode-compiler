CLASS Trace
    Id : INTEGER
    PROCEDURE DELETE()
        OUTPUT "destroy:", Id
    ENDPROCEDURE
ENDCLASS
PROCEDURE Inner()
    DECLARE A : Trace
    A.Id <- 1
    DECLARE B : Trace
    B.Id <- 2
    THROW
ENDPROCEDURE
PROCEDURE Outer()
    DECLARE A : Trace
    A.Id <- 3
    CALL Inner()
ENDPROCEDURE
PROCEDURE Check(I : INTEGER)
    IF I = 2 THEN
        THROW
    ENDIF
ENDPROCEDURE
PROCEDURE Loop()
    DECLARE I : INTEGER
    FOR I <- 1 TO 2
        CALL Check(I)
        DECLARE Late : Trace
        Late.Id <- 60
    NEXT I
ENDPROCEDURE
PROCEDURE LocalCatch()
    DECLARE Kept : Trace
    Kept.Id <- 70
    TRY
        THROW
    CATCH
        OUTPUT "local caught"
    ENDTRY
    OUTPUT "still alive"
ENDPROCEDURE
DECLARE MainObject : Trace
MainObject.Id <- 100
TRY
    CALL Outer()
CATCH
    OUTPUT "caught"
ENDTRY
TRY
    CALL Loop()
CATCH
    OUTPUT "loop caught"
ENDTRY
CALL LocalCatch()
OUTPUT "uncaught next"
CALL Outer()
