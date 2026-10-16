DECLARE Empty : STRING
DECLARE Text : STRING
Empty <- ""
Text <- "ABCDEFGH"
OUTPUT LENGTH(Empty), ":", LENGTH(Text)
OUTPUT MID(Text, 2, 3), ":", RIGHT(Text, 3)
OUTPUT "Summer" & " " & "Pudding"
OUTPUT LCASE('W'), ":", UCASE('h'), ":", LCASE('@')
OUTPUT "same" = ("sa" & "me"), ":", "A" < "B"
