CLASS STRING
    Text : C_STRING
    PROCEDURE NEW(Value : C_STRING)
        Text <- Value
    ENDPROCEDURE
    FUNCTION C_STR() RETURNS C_STRING
        RETURN Text
    ENDFUNCTION
ENDCLASS
DECLARE Explicit : STRING("explicit")
OUTPUT Explicit
DECLARE Text : STRING
Text <- "requires opt-in"
OUTPUT "requires opt-in"
