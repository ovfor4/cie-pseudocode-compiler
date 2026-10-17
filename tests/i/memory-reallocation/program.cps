TYPE Raw = C_TYPE("void*")
TYPE NativePointer = C_TYPE("int*")
TYPE Pair
    DECLARE First : INTEGER
    DECLARE Second : INTEGER
ENDTYPE
TYPE PairPointer = ^Pair
TYPE IntPointer = ^INTEGER
DECLARE Empty : Raw
DECLARE P : Raw
DECLARE Q : Raw
DECLARE Number : INTEGER
DECLARE Pointer : IntPointer
DECLARE PairAddress : PairPointer
DECLARE Native : NativePointer
OUTPUT REALLOC(Empty, 0) = Empty
P <- REALLOC(Empty, 16)
PairAddress <- (PairPointer)P
PairAddress^.First <- 12
PairAddress^.Second <- 34
P <- REALLOC(P, 32)
PairAddress <- (PairPointer)P
OUTPUT PairAddress^.First, ":", PairAddress^.Second
P <- REALLOC(P, (C_INT)8)
Pointer <- (IntPointer)P
OUTPUT Pointer^
TRY
    P <- REALLOC(P, -1)
CATCH
    Pointer <- (IntPointer)P
    OUTPUT "preserved:", Pointer^
ENDTRY
Pointer^ <- 56
P <- REALLOC(P, 16)
Pointer <- (IntPointer)P
OUTPUT Pointer^
Q <- P
OUTPUT REALLOC(P, 0) = Empty
TRY
    REALLOC(Q, 8)
CATCH
    OUTPUT "released address caught"
ENDTRY
Pointer <- ^Number
Q <- (Raw)Pointer
TRY
    REALLOC(Q, 0)
CATCH
    OUTPUT "invalid zero resize caught"
ENDTRY
FUNCTION AllocateNative() RETURNS Raw
    RETURN MALLOC(16)
ENDFUNCTION
P <- AllocateNative()
Native <- (NativePointer)P
Native^ <- (C_INT)71
P <- REALLOC(P, 32)
Native <- (NativePointer)P
OUTPUT Native^
FREE(P)
// A runtime failure raised from a function propagates to the caller.
FUNCTION InvalidResize(Value : Raw) RETURNS Raw
    RETURN REALLOC(Value, 8)
ENDFUNCTION
TRY
    InvalidResize(Q)
CATCH
    TRY
        FREE(Q)
    CATCH
        OUTPUT "nested catch"
    ENDTRY
ENDTRY
// STRING storage is deliberately outside the explicit allocation list.
Q <- (Raw)(C_STRING)"internal"
TRY
    REALLOC(Q, 16)
CATCH
    OUTPUT "internal buffer caught"
ENDTRY
FREE(REALLOC(Empty, (C_INT)8))
