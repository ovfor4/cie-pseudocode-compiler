DECLARE X : INTEGER
X <- 4
IF X > 0 THEN
    IF X = 4 THEN
        OUTPUT "nested"
    ELSE
        OUTPUT "wrong"
    ENDIF
ENDIF
CASE OF X
    1 TO 5 : OUTPUT "first"
    4 : OUTPUT "wrong"
    OTHERWISE : OUTPUT "wrong"
ENDCASE
X <- 9
CASE OF X
    1 TO 5 : OUTPUT "wrong"
    OTHERWISE : OUTPUT "otherwise"
ENDCASE
