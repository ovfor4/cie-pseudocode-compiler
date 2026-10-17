INCLUDE "nested/values.cps"
CLASS Container
    Value : INTEGER
    FUNCTION Read RETURNS INTEGER
        RETURN Value + Base
    ENDFUNCTION
ENDCLASS
TYPE Pair
    DECLARE Value : INTEGER
ENDTYPE
