INCLUDE "lib.cps"
TYPE Score
    DECLARE Value : INTEGER
ENDTYPE
OVERLOAD FUNCTION OPERATOR>(Left : Score, Right : Score) RETURNS BOOLEAN
    RETURN Left.Value > Right.Value
ENDFUNCTION
DECLARE Low : Score
DECLARE High : Score
Low.Value <- 3
High.Value <- 9
DECLARE Best : Score
Best <- Max(Low, High)
OUTPUT Best.Value
