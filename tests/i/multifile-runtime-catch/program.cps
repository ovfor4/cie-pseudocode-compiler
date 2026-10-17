INCLUDE "lib.cps"
TRY
    OUTPUT Quotient(0)
CATCH
    OUTPUT "runtime caught"
ENDTRY
OUTPUT Quotient(2)
