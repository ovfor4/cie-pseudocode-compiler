INCLUDE "library.cps"
TRY
    CALL Check()
CATCH
    OUTPUT "bounds caught"
ENDTRY
TRY
    CALL Bounds()
CATCH
    OUTPUT "contract caught"
ENDTRY
TRY
    CALL FileError()
CATCH
    OUTPUT "file caught"
ENDTRY
TRY
    DECLARE X : Broken
CATCH
    OUTPUT "constructor caught"
ENDTRY
