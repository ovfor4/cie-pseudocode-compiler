CLASS Leaf
    Value : INTEGER
    PROCEDURE New()
        OUTPUT "leaf:", Value
        Value <- 3
    ENDPROCEDURE
ENDCLASS
CLASS Pair
    First : Leaf
    Second : Leaf
    PROCEDURE NEW()
        OUTPUT "pair:", First.Value + Second.Value
    ENDPROCEDURE
ENDCLASS
TYPE Wrapped
    DECLARE Item : Pair
ENDTYPE
CLASS Named
    Text : STRING
    PROCEDURE NEW(Name : STRING)
        Text <- Name
    ENDPROCEDURE
ENDCLASS
CLASS Plain
    Value : INTEGER
ENDCLASS
DECLARE A : Leaf
DECLARE B : Leaf()
DECLARE N : Named("name")
OUTPUT N.Text
DECLARE P : Plain()
OUTPUT P.Value
DECLARE Items : ARRAY[-1:0, 2:3] OF Leaf
OUTPUT Items[-1,2].Value, ",", Items[0,3].Value
DECLARE W : Wrapped
OUTPUT W.Item.First.Value, ",", W.Item.Second.Value
DECLARE Index : INTEGER
FOR Index <- 1 TO 2
    DECLARE Local : Leaf
    OUTPUT Local.Value
    Local.Value <- 99
NEXT Index
