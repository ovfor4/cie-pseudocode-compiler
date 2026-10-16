DECLARE X : INTEGER
DECLARE Values : ARRAY[1:1] OF INTEGER
TYPE IntPtr = ^INTEGER
DECLARE P : IntPtr
X <- 0
TRY
    OUTPUT 4 DIV X
CATCH
    OUTPUT "division caught"
ENDTRY
TRY
    OUTPUT Values[2]
CATCH
    OUTPUT "bounds caught"
ENDTRY
TRY
    OUTPUT P^
CATCH
    OUTPUT "pointer caught"
ENDTRY
TRY
    CLOSEFILE "closed.txt"
CATCH
    OUTPUT "file caught"
ENDTRY
TRY
    THROW
CATCH
    OUTPUT "throw caught"
ENDTRY
OUTPUT "alive"
