CLASS Value
    N : INTEGER
    OVERLOAD PROCEDURE NEW()
    ENDPROCEDURE
    OVERLOAD PROCEDURE NEW(BYREF Other : Value)
        N <- Other.N
    ENDPROCEDURE
    OVERLOAD PROCEDURE OPERATOR<-(BYREF Other : Value)
        OUTPUT "copy:", Other.N
        N <- Other.N
    ENDPROCEDURE
    OVERLOAD PROCEDURE OPERATOR<-(Text : C_STRING)
        N <- LENGTH((STRING)Text)
    ENDPROCEDURE
ENDCLASS
FUNCTION Make() RETURNS Value
    DECLARE X : Value
    X.N <- 7
    RETURN X
ENDFUNCTION
TYPE ValuePtr = ^Value
TYPE Box
    DECLARE Item : Value
ENDTYPE
DECLARE A : Value
DECLARE B : Value
A.N <- 3
B <- A
B <- B
B <- Make()
A <- (C_STRING)"hello"
DECLARE R : Box
R.Item <- B
DECLARE Items : ARRAY[1:1] OF Value
Items[1] <- A
DECLARE P : ValuePtr
P <- ^A
P^ <- B
OUTPUT A.N, ":", B.N, ":", R.Item.N, ":", Items[1].N
