TYPE Pair
    DECLARE X : INTEGER
    DECLARE Y : INTEGER
ENDTYPE
TYPE PairPtr = ^Pair
PROCEDURE IgnoreCopy(P : Pair)
    P.X <- 999
ENDPROCEDURE
PROCEDURE Shift(BYREF X : INTEGER, Y : INTEGER)
    X <- X + 1
    Y <- Y - 1
ENDPROCEDURE
FUNCTION Triangle(N : INTEGER) RETURNS INTEGER
    IF N = 0 THEN
        RETURN 0
    ENDIF
    RETURN N + Triangle(N - 1)
ENDFUNCTION
DECLARE Count : INTEGER
DECLARE Index : INTEGER
DECLARE N : INTEGER
DECLARE Value : Pair
DECLARE Saved : Pair
DECLARE P : PairPtr
INPUT Count
FOR Index <- 1 TO Count
    INPUT N
    DECLARE X : INTEGER
    DECLARE Y : INTEGER
    INPUT X
    INPUT Y
    Value.X <- X
    Value.Y <- Y
    Saved <- Value
    CALL IgnoreCopy(Value)
    P <- ^Value
    P^.X <- P^.X + P^.Y
    CALL Shift(X, Y)
    OUTPUT Value.X, ":", Saved.X, ":", X, ":", Y, ":", Triangle(N)
NEXT Index
