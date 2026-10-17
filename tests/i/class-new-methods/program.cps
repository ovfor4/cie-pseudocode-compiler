CLASS Box
    Value : STRING
    PROCEDURE NEW(Text : STRING)
        Value <- Text
    ENDPROCEDURE
    PROCEDURE Reset()
        this.NEW("again")
    ENDPROCEDURE
ENDCLASS
DECLARE Obj : Box
Obj.NEW("x")
OUTPUT Obj.Value
CALL Obj.new("call")
OUTPUT Obj.Value
Obj.Reset()
OUTPUT Obj.Value
