OVERLOAD FUNCTION Score(Values : ARRAY OF INTEGER, First : INTEGER, Size : INTEGER) RETURNS INTEGER
    DECLARE I : INTEGER
    DECLARE Total : INTEGER
    Total <- 0
    FOR I <- First TO First + Size - 1
        Total <- Total + Values[I]
    NEXT I
    RETURN Total
ENDFUNCTION
OVERLOAD FUNCTION Score(Values : ARRAY OF REAL, First : INTEGER, Size : INTEGER) RETURNS BOOLEAN
    DECLARE I : INTEGER
    DECLARE Total : REAL
    Total <- 0.0
    FOR I <- First TO First + Size - 1
        Total <- Total + Values[I]
    NEXT I
    RETURN Total > 0.0
ENDFUNCTION
OVERLOAD FUNCTION Score(Values : ARRAY[1:2,1:2] OF INTEGER, First : INTEGER, Size : INTEGER) RETURNS INTEGER
    RETURN Values[1,1] + 2 * Values[1,2] + 3 * Values[2,1] + 4 * Values[2,2]
ENDFUNCTION
