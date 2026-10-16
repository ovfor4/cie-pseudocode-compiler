DECLARE Count : INTEGER
DECLARE Index : INTEGER
DECLARE A : INTEGER
DECLARE B : INTEGER
DECLARE Small : INTEGER
DECLARE Sign : INTEGER
DECLARE Band : INTEGER
INPUT Count
FOR Index <- 1 TO Count
    INPUT A
    INPUT B
    Small <- A MOD 1000001
    IF A < 0 THEN
        Sign <- -1
    ELSE
        IF A = 0 THEN
            Sign <- 0
        ELSE
            Sign <- 1
        ENDIF
    ENDIF
    CASE OF Small
        -10 TO 10 : Band <- 2
        0 : Band <- 99
        OTHERWISE : Band <- 3
    ENDCASE
    OUTPUT A < B, ":", A DIV B, ":", A MOD B, ":", Small + B, ":", Small - B, ":", Small * B, ":", Sign, ":", Band
NEXT Index
