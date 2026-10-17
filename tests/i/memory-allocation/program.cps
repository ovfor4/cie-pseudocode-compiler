TYPE Raw = C_TYPE("void*")
TYPE IntPointer = ^INTEGER
TYPE Pair
    DECLARE First : INTEGER
    DECLARE Second : INTEGER
ENDTYPE
TYPE PairPointer = ^Pair
DECLARE Empty : Raw
DECLARE P : Raw
DECLARE Q : Raw
DECLARE R : Raw
DECLARE Number : INTEGER
DECLARE Pointer : IntPointer
DECLARE PairAddress : PairPointer
DECLARE Result : Raw
OUTPUT MALLOC(0) = Empty
OUTPUT FREE(Empty) = Empty
P <- malloc(8)
Pointer <- (IntPointer)P
Pointer^ <- 42
OUTPUT Pointer^
Q <- MALLOC((C_INT)16)
PairAddress <- (PairPointer)Q
PairAddress^.First <- 11
PairAddress^.Second <- 22
OUTPUT PairAddress^.First, ":", PairAddress^.Second
R <- MALLOC(1)
// Remove a middle node, then the head, then the tail.
Result <- FREE(Q)
OUTPUT Result = Q
FREE(R)
FREE(P)
TRY
    FREE(P)
CATCH
    OUTPUT "double free caught"
ENDTRY
Pointer <- ^Number
TRY
    FREE((Raw)Pointer)
CATCH
    OUTPUT "stack address caught"
ENDTRY
Q <- MALLOC(16)
PairAddress <- (PairPointer)Q
Pointer <- ^PairAddress^.Second
TRY
    FREE((Raw)Pointer)
CATCH
    OUTPUT "interior address caught"
ENDTRY
FREE(Q)
TRY
    MALLOC(-1)
CATCH
    OUTPUT "allocation failure caught"
ENDTRY
// Allocation still works after all caught failures.
P <- MALLOC(8)
Pointer <- (IntPointer)P
Pointer^ <- 99
OUTPUT Pointer^
FREE(P)
