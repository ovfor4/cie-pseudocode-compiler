CLASS Box
    PRIVATE Hidden : INTEGER
    PUBLIC Value : INTEGER
    PROCEDURE Set(Number : INTEGER)
        Hidden <- Number
    ENDPROCEDURE
    PRIVATE FUNCTION Secret() RETURNS INTEGER
        RETURN Hidden
    ENDFUNCTION
    PROCEDURE BadLocal()
        DECLARE this : Box
    ENDPROCEDURE
    PROCEDURE BadParam(this : Box)
    ENDPROCEDURE
    PROCEDURE NEW()
    ENDPROCEDURE
    PROCEDURE set()
    ENDPROCEDURE
    FUNCTION Value() RETURNS INTEGER
        RETURN 0
    ENDFUNCTION
ENDCLASS
CLASS Stranger
    PROCEDURE Probe(Other : Box)
        OUTPUT Other.Hidden
        OUTPUT Other.Secret()
    ENDPROCEDURE
ENDCLASS
DECLARE B : Box
OUTPUT B.Secret()
OUTPUT B.Hidden
B.Set()
B.Missing()
DECLARE Number : INTEGER
Number.Set(1)
