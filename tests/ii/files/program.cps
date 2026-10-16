DECLARE Count : INTEGER
DECLARE Index : INTEGER
DECLARE Text : STRING
DECLARE Extra : STRING
DECLARE Line : STRING
DECLARE Combined : STRING
DECLARE Lines : INTEGER
INPUT Count
FOR Index <- 1 TO Count
    INPUT Text
    INPUT Extra
    OPENFILE "roundtrip.txt" FOR WRITE
    WRITEFILE "roundtrip.txt", Text
    WRITEFILE "roundtrip.txt", ""
    CLOSEFILE "roundtrip.txt"
    OPENFILE "roundtrip.txt" FOR APPEND
    WRITEFILE "roundtrip.txt", Extra
    CLOSEFILE "roundtrip.txt"
    OPENFILE "roundtrip.txt" FOR READ
    Combined <- ""
    Lines <- 0
    WHILE NOT EOF("roundtrip.txt")
        READFILE "roundtrip.txt", Line
        Combined <- Combined & "[" & Line & "]"
        Lines <- Lines + 1
    ENDWHILE
    CLOSEFILE "roundtrip.txt"
    OUTPUT Lines, ":", Combined
NEXT Index
