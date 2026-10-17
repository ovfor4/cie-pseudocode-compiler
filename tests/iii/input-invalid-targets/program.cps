TYPE Student
    DECLARE Score : INTEGER
ENDTYPE
CLASS Box
    PRIVATE Hidden : INTEGER
ENDCLASS
TYPE StudentPtr = ^Student
TYPE Season = (Spring, Summer)
CONSTANT Limit = 3
DECLARE Scores : ARRAY[1:2] OF INTEGER
DECLARE Item : Box
DECLARE Person : Student
DECLARE P : StudentPtr
DECLARE S : Season
DECLARE Text : C_STRING
INPUT Limit
INPUT Item.Hidden
INPUT Scores
INPUT Person
INPUT P
INPUT S
INPUT Text
