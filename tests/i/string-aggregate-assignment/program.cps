GLOBAL
    DECLARE Destroyed : INTEGER
    DECLARE Copies : INTEGER
ENDGLOBAL
CLASS Box
    Text : STRING
    OVERLOAD PROCEDURE NEW()
    ENDPROCEDURE
    OVERLOAD PROCEDURE NEW(BYREF Other : Box)
        Text <- Other.Text
        Copies <- Copies + 1
    ENDPROCEDURE
    PROCEDURE DELETE()
        Destroyed <- Destroyed + 1
    ENDPROCEDURE
ENDCLASS
PROCEDURE Check()
    DECLARE A : Box
    DECLARE B : Box
    A.Text <- "left"
    B.Text <- "right"
    B <- A
    B <- B
    B.Text <- "changed"
    OUTPUT A.Text, ":", B.Text, ":", Destroyed, ":", Copies
ENDPROCEDURE
CALL Check()
OUTPUT Destroyed, ":", Copies
