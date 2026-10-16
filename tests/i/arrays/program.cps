DECLARE Grid : ARRAY[-1:1,2:3] OF INTEGER
DECLARE Row : INTEGER
DECLARE Col : INTEGER
DECLARE Total : INTEGER
Total <- 0
FOR Row <- -1 TO 1
    FOR Col <- 2 TO 3
        Grid[Row,Col] <- Row * 10 + Col
        Total <- Total + Grid[Row,Col]
    NEXT Col
NEXT Row
OUTPUT Grid[-1,2], ":", Grid[1,3], ":", Total
