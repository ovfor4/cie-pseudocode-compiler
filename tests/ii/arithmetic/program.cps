DECLARE Count : INTEGER
DECLARE Index : INTEGER
DECLARE A : INTEGER
DECLARE B : INTEGER
DECLARE Small : INTEGER
INPUT Count
FOR Index <- 1 TO Count
    INPUT A
    INPUT B
    Small <- A MOD 1000001
    OUTPUT A < B, ":", A DIV B, ":", A MOD B, ":", Small + B, ":", Small - B, ":", Small * B
NEXT Index
