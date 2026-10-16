DECLARE Count : INTEGER
DECLARE Index : INTEGER
DECLARE Empty : INTEGER
DECLARE Text : STRING
DECLARE Start : INTEGER
DECLARE Width : INTEGER
INPUT Count
FOR Index <- 1 TO Count
    INPUT Empty
    INPUT Text
    INPUT Start
    INPUT Width
    IF Empty = 1 THEN
        Text <- ""
    ENDIF
    OUTPUT LENGTH(Text), ":", MID(Text, Start, Width), ":", RIGHT(Text, Width), ":", Text = ("" & Text), ":", LCASE(Text), ":", UCASE(Text)
NEXT Index
