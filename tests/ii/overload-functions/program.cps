INCLUDE "scalar.cps"
INCLUDE "arrays.cps"
DECLARE Count : INTEGER
DECLARE Row : INTEGER
DECLARE A : INTEGER
DECLARE B : INTEGER
DECLARE Size : INTEGER
DECLARE Lower : INTEGER
DECLARE Text : STRING
DECLARE I : INTEGER
DECLARE Changed : INTEGER
INPUT Count
FOR Row <- 1 TO Count
    INPUT A
    INPUT B
    INPUT Size
    INPUT Lower
    INPUT Text
    DECLARE Integers : ARRAY[Lower:Lower+Size-1] OF INTEGER
    DECLARE Reals : ARRAY[Lower:Lower+Size-1] OF REAL
    DECLARE Matrix : ARRAY[1:2,1:2] OF INTEGER
    FOR I <- Lower TO Lower + Size - 1
        Integers[I] <- A + (I - Lower) * B
        Reals[I] <- (A + (I - Lower) * B) / 8
    NEXT I
    Matrix[1,1] <- A
    Matrix[1,2] <- B
    Matrix[2,1] <- A + B
    Matrix[2,2] <- A - B
    Changed <- A
    CALL sCoRe(Changed, B, Size)
    OUTPUT Score(A), ":", Score(A / 8), ":", Score(Text), ":", Score(A, B), ":", Score(Integers, Lower, Size), ":", Score(Reals, Lower, Size), ":", Score(Matrix, Lower, Size), ":", Score(Score(A)), ":", Changed, ":", A
NEXT Row
