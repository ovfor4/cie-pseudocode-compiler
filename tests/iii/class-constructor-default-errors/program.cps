CLASS Required
    PROCEDURE NEW(Number : INTEGER)
    ENDPROCEDURE
ENDCLASS
CLASS Plain
    Value : INTEGER
ENDCLASS
CLASS Hidden
    PRIVATE PROCEDURE NEW()
    ENDPROCEDURE
ENDCLASS
TYPE Wrapped
    DECLARE Item : Required
ENDTYPE
DECLARE Missing : Required
DECLARE EmptyArgs : Required()
DECLARE Extra : Plain(1)
DECLARE PrivateObject : Hidden
DECLARE Items : ARRAY[1:2] OF Required
DECLARE Nested : Wrapped
DECLARE Scalar : INTEGER()
