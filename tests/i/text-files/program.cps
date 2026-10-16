DECLARE Line : STRING
DECLARE Count : INTEGER
OPENFILE "input.txt" FOR READ
Count <- 0
WHILE NOT EOF("input.txt")
    READFILE "input.txt", Line
    OUTPUT "[", Line, "]"
    Count <- Count + 1
ENDWHILE
CLOSEFILE "input.txt"
OPENFILE "empty.txt" FOR READ
OUTPUT EOF("empty.txt"), ":", Count
CLOSEFILE "empty.txt"
OPENFILE "result.txt" FOR WRITE
WRITEFILE "result.txt", "first"
WRITEFILE "result.txt", ""
CLOSEFILE "result.txt"
OPENFILE "result.txt" FOR APPEND
WRITEFILE "result.txt", "last"
CLOSEFILE "result.txt"
OPENFILE "result.txt" FOR READ
WHILE NOT EOF("result.txt")
    READFILE "result.txt", Line
    OUTPUT "[", Line, "]"
ENDWHILE
CLOSEFILE "result.txt"
