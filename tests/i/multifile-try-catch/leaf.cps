GLOBAL
    DECLARE Destroyed : INTEGER
ENDGLOBAL
CLASS Guard
    Id : INTEGER
    PROCEDURE DELETE()
        OUTPUT "destroy:", Id
        Destroyed <- Destroyed + 1
    ENDPROCEDURE
ENDCLASS
PROCEDURE Raise()
    DECLARE GuardTwo : Guard
    GuardTwo.Id <- 2
    THROW
ENDPROCEDURE
