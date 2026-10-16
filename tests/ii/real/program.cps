FUNCTION Twice(Value : REAL) RETURNS REAL
    RETURN Value * 2.0
ENDFUNCTION
DECLARE Count : INTEGER
DECLARE Index : INTEGER
DECLARE A : INTEGER
DECLARE B : INTEGER
DECLARE X : REAL
DECLARE Y : REAL
INPUT Count
FOR Index <- 1 TO Count
    INPUT A
    INPUT B
    X <- A / 8
    Y <- B / 8
    OUTPUT X < Y, ":", X = Y, ":", Twice(X) = (A / 4), ":", (X + Y) = ((A + B) / 8), ":", (X * Y) = ((A * B) / 64)
NEXT Index
