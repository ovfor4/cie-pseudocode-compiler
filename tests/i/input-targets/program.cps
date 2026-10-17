TYPE Student
    DECLARE Score : INTEGER
    DECLARE Name : STRING
    DECLARE Passed : BOOLEAN
    DECLARE Grade : CHAR
    DECLARE Average : REAL
    DECLARE Native : C_INT
ENDTYPE
CLASS Box
    Value : INTEGER
    Details : Student
ENDCLASS
TYPE StudentPtr = ^Student
DECLARE Scores : ARRAY[1:3] OF INTEGER
DECLARE Grid : ARRAY[-1:0,2:3] OF INTEGER
DECLARE Students : ARRAY[1:2] OF Student
DECLARE Item : Box
DECLARE P : StudentPtr
DECLARE j : INTEGER
FOR j <- 1 TO 3
    INPUT Scores[j]
NEXT j
INPUT Grid[-1,3]
INPUT Students[2].Score
INPUT Students[2].Name
INPUT Students[2].Passed
INPUT Students[2].Grade
INPUT Students[2].Average
INPUT Students[2].Native
INPUT Item.Value
INPUT Item.Details.Score
P <- ^Students[2]
INPUT P^.Score
OUTPUT Scores[1], ":", Scores[2], ":", Scores[3], ":", Grid[-1,3]
OUTPUT Students[2].Score, ":", Students[2].Name, ":", Students[2].Passed, ":", Students[2].Grade
OUTPUT Students[2].Average = 2.5, ":", Students[2].Native
OUTPUT Item.Value, ":", Item.Details.Score
FUNCTION NextIndex() RETURNS INTEGER
    OUTPUT "index evaluated"
    RETURN 1
ENDFUNCTION
INPUT Scores[NextIndex()]
OUTPUT Scores[1]
TRY
    INPUT Scores[4]
CATCH
    OUTPUT "bounds caught"
ENDTRY
DECLARE Unset : StudentPtr
TRY
    INPUT Unset^.Score
CATCH
    OUTPUT "pointer caught"
ENDTRY
INPUT Scores[1]
OUTPUT Scores[1]
