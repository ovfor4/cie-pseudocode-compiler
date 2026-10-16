PROCEDURE Change(BYREF A : INTEGER, B : INTEGER, BYVAL C : INTEGER)
    A <- A + 1
    B <- B + 2
    C <- C + 100
ENDPROCEDURE
FUNCTION Factorial(N : INTEGER) RETURNS INTEGER
    IF N <= 1 THEN
        RETURN 1
    ENDIF
    RETURN N * Factorial(N - 1)
ENDFUNCTION
PROCEDURE LocalCopy(Values : ARRAY OF INTEGER)
    Values[1] <- 99
ENDPROCEDURE
PROCEDURE Shared(BYREF Values : ARRAY[1:2] OF INTEGER)
    Values[2] <- Values[1] + 3
ENDPROCEDURE
DECLARE A : INTEGER
DECLARE B : INTEGER
DECLARE C : INTEGER
DECLARE Values : ARRAY[1:2] OF INTEGER
A <- 1
B <- 2
C <- 3
CALL Change(A, B, C)
Values[1] <- 4
Values[2] <- 5
CALL LocalCopy(Values)
CALL Shared(Values)
OUTPUT A, ":", B, ":", C, ":", Factorial(5)
OUTPUT Values[1], ":", Values[2]
