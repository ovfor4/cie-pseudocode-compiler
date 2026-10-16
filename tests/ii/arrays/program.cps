DECLARE Count : INTEGER
DECLARE Record : INTEGER
DECLARE Lower : INTEGER
DECLARE Size : INTEGER
DECLARE Step : INTEGER
DECLARE Base : INTEGER
DECLARE I : INTEGER
DECLARE Sum : INTEGER
INPUT Count
FOR Record <- 1 TO Count
    INPUT Lower
    INPUT Size
    INPUT Step
    INPUT Base
    DECLARE Values : ARRAY[Lower:Lower+Size-1] OF INTEGER
    Sum <- 0
    FOR I <- Lower TO Lower + Size - 1
        Values[I] <- Base + (I - Lower) * Step
        Sum <- Sum + Values[I]
    NEXT I
    OUTPUT Sum, ":", Values[Lower], ":", Values[Lower+Size-1]
NEXT Record
