INCLUDE "middle.cps"
TRY
    CALL Outer()
CATCH
    OUTPUT "caught:", Destroyed
ENDTRY
OUTPUT "continued"
TRY
    CALL CatchHere()
    OUTPUT "returned"
CATCH
    OUTPUT "wrong handler"
ENDTRY
