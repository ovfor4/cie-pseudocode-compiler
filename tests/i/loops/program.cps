DECLARE I : INTEGER
DECLARE Total : INTEGER
Total <- 0
FOR I <- 3 TO 1
    Total <- Total + 1000
NEXT I
FOR I <- 2 TO 2
    Total <- Total + I
NEXT I
FOR I <- 3 TO -1 STEP -2
    Total <- Total + I
NEXT I
WHILE FALSE
    Total <- Total + 1000
ENDWHILE
I <- 0
WHILE I < 3
    Total <- Total + I
    I <- I + 1
ENDWHILE
REPEAT
    Total <- Total + 10
UNTIL TRUE
I <- 0
REPEAT
    I <- I + 1
UNTIL I = 3
OUTPUT Total, ":", I
