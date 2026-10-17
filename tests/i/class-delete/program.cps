CLASS Leaf
    Value : INTEGER
    PROCEDURE DELETE()
        OUTPUT "leaf:", Value
    ENDPROCEDURE
ENDCLASS
CLASS Pair
    Left : Leaf
    Right : Leaf
    PROCEDURE NEW()
        Left.Value <- 1
        Right.Value <- 2
    ENDPROCEDURE
    PROCEDURE DELETE()
        OUTPUT "pair"
    ENDPROCEDURE
ENDCLASS
DECLARE Obj : Pair
Obj <- NEW Pair()
Obj.DELETE()
CALL Obj.DELETE()
DELETE Obj
TRY
    DELETE Obj
CATCH
    OUTPUT "double caught"
ENDTRY
DECLARE Stack : Pair
TRY
    DELETE Stack
CATCH
    OUTPUT "stack caught"
ENDTRY
DECLARE Items : ARRAY[1:2] OF Pair
Items[2].Right.DELETE()
