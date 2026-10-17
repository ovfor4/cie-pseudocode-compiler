INCLUDE "leaf.cps"
PROCEDURE Outer()
    DECLARE GuardOne : Guard
    GuardOne.Id <- 1
    CALL Raise()
ENDPROCEDURE
PROCEDURE CatchHere()
    TRY
        CALL Raise()
    CATCH
        OUTPUT "library caught:", Destroyed
    ENDTRY
ENDPROCEDURE
