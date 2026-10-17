CLASS Box
    Value : INTEGER
    PROCEDURE NEW()
        Value <- 1
    ENDPROCEDURE
    PROCEDURE Reset()
        this.NEW()
    ENDPROCEDURE
    PROCEDURE ReplaceSelf()
        this <- NEW Box()
    ENDPROCEDURE
ENDCLASS
CLASS Named
    Value : INTEGER
    PROCEDURE NEW(Number : INTEGER)
        Value <- Number
    ENDPROCEDURE
ENDCLASS
CLASS Holder
    Item : Box
    PROCEDURE ReplaceField()
        Item <- NEW Box
    ENDPROCEDURE
ENDCLASS
CLASS Failing
    PROCEDURE NEW()
        THROW
    ENDPROCEDURE
ENDCLASS
TYPE BoxPointer = ^Box
PROCEDURE ReplaceRef(BYREF Item : Box)
    Item <- NEW Box
ENDPROCEDURE
PROCEDURE ReplaceVal(Item : Box)
    DECLARE Old : BoxPointer
    Old <- ^Item
    Item <- NEW Box()
    Item.Value <- 20
    OUTPUT "byval:", Old^.Value, ",", Item.Value
ENDPROCEDURE
DECLARE A : Box
DECLARE B : Box
DECLARE OldA : BoxPointer
DECLARE CurrentA : BoxPointer
OldA <- ^A
A.Value <- 7
A <- NEW Box
A.Value <- 9
CurrentA <- ^A
OUTPUT "rebind:", OldA^.Value, ",", A.Value
B.Value <- 4
A <- B
OUTPUT "copy:", OldA^.Value, ",", CurrentA^.Value, ",", B.Value
CALL ReplaceVal(A)
OUTPUT "caller:", A.Value
CALL ReplaceRef(A)
OUTPUT "byref:", CurrentA^.Value, ",", A.Value
A.Value <- 11
A.ReplaceSelf()
OUTPUT "this:", CurrentA^.Value, ",", A.Value
A.Value <- 12
A.Reset()
OUTPUT "reset:", A.Value
DECLARE Items : ARRAY[1:2] OF Box
DECLARE Element : BoxPointer
Element <- ^Items[1]
Items[1].Value <- 30
Items[1] <- NEW Box()
OUTPUT "array:", Element^.Value, ",", Items[2].Value
DECLARE H : Holder
DECLARE Field : BoxPointer
Field <- ^H.Item
H.Item.Value <- 40
H.Item <- NEW Box
OUTPUT "field:", Field^.Value
H.Item.Value <- 41
H.ReplaceField()
OUTPUT "alias:", Field^.Value
CurrentA^ <- NEW Box
OUTPUT "deref:", A.Value
DECLARE N : Named(0)
N <- NEW Named(42)
OUTPUT "args:", N.Value
TRY
    DECLARE Broken : Failing
CATCH
    OUTPUT "declare caught"
ENDTRY
TRY
    Broken <- NEW Failing
CATCH
    OUTPUT "new caught"
ENDTRY
DECLARE Index : INTEGER
FOR Index <- 1 TO 2
    DECLARE Local : Box
    OUTPUT "loop:", Local.Value
    Local <- NEW Box
    Local.Value <- 99
NEXT Index
