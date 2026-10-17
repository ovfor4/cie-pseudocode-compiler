CLASS Meter
    PRIVATE Value : INTEGER
    PUBLIC OVERLOAD PROCEDURE NEW()
        Value <- 5
    ENDPROCEDURE
    PUBLIC OVERLOAD PROCEDURE NEW(X : INTEGER)
        Value <- 2 * X + 3
    ENDPROCEDURE
    PUBLIC OVERLOAD PROCEDURE NEW(Text : STRING)
        Value <- LENGTH(Text) + 20
    ENDPROCEDURE
    PUBLIC OVERLOAD PROCEDURE NEW(X : INTEGER, Y : INTEGER)
        Value <- X - Y
    ENDPROCEDURE
    PUBLIC OVERLOAD FUNCTION Get() RETURNS INTEGER
        RETURN Value
    ENDFUNCTION
    PUBLIC OVERLOAD FUNCTION Get(X : INTEGER) RETURNS INTEGER
        RETURN Value + 3 * X
    ENDFUNCTION
    PUBLIC OVERLOAD FUNCTION Get(X : REAL) RETURNS BOOLEAN
        RETURN Value > X
    ENDFUNCTION
    PUBLIC OVERLOAD FUNCTION Get(Text : STRING) RETURNS STRING
        RETURN Text & "!"
    ENDFUNCTION
    PUBLIC OVERLOAD PROCEDURE Set(X : INTEGER)
        Value <- X
    ENDPROCEDURE
    PUBLIC OVERLOAD PROCEDURE Set(Text : STRING)
        Value <- LENGTH(Text)
    ENDPROCEDURE
    PUBLIC PROCEDURE Apply(X : INTEGER)
        CALL this.Set(this.Get(X))
    ENDPROCEDURE
ENDCLASS
TYPE Holder
    DECLARE Item : Meter
ENDTYPE
DECLARE Count : INTEGER
DECLARE Row : INTEGER
DECLARE A : INTEGER
DECLARE B : INTEGER
DECLARE Size : INTEGER
DECLARE Text : STRING
DECLARE I : INTEGER
DECLARE Total : INTEGER
INPUT Count
FOR Row <- 1 TO Count
    INPUT A
    INPUT B
    INPUT Size
    INPUT Text
    DECLARE DefaultItem : Meter
    DECLARE NumberItem : Meter(A)
    DECLARE TextItem : Meter(Text)
    DECLARE PairItem : Meter(A, B)
    DECLARE Wrapped : Holder
    DECLARE Items : ARRAY[1:Size] OF Meter
    DECLARE ChangedNumber : Meter(A)
    DECLARE ChangedText : Meter(Text)
    DECLARE Rebuilt : Meter(A, B)
    DECLARE HeapItem : Meter
    Total <- 0
    FOR I <- 1 TO Size
        Total <- Total + Items[I].Get()
    NEXT I
    HeapItem <- NEW Meter(A, B)
    CALL ChangedNumber.Set(Text)
    CALL ChangedText.Set(A)
    CALL ChangedText.Apply(B)
    CALL Rebuilt.NEW(Text)
    OUTPUT DefaultItem.Get(), ":", NumberItem.Get(), ":", TextItem.Get(), ":", PairItem.Get(), ":", Wrapped.Item.Get(), ":", Total, ":", NumberItem.Get(B), ":", NumberItem.Get(B / 8), ":", NumberItem.Get(Text), ":", HeapItem.Get(), ":", ChangedNumber.Get(), ":", ChangedText.Get(), ":", Rebuilt.Get()
    DELETE HeapItem
NEXT Row
