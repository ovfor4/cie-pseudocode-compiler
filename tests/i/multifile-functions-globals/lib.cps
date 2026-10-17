CONSTANT Offset = 2
FUNCTION Add(a : INTEGER, b : INTEGER) RETURNS INTEGER
    RETURN a + b
ENDFUNCTION
PROCEDURE Increment()
    Counter <- Counter + Offset
ENDPROCEDURE
PROCEDURE Shadow()
    DECLARE Counter : INTEGER
    Counter <- 40
    OUTPUT Counter
ENDPROCEDURE
FUNCTION ReadCounter RETURNS INTEGER
    RETURN Counter
ENDFUNCTION
GLOBAL
    DECLARE Counter : INTEGER
    Counter <- 3
    DECLARE Label : STRING
    Label <- "shared"
    DECLARE Enabled : BOOLEAN
    Enabled <- TRUE
    DECLARE Initial : CHAR
    Initial <- 'Q'
    DECLARE Ratio : REAL
    Ratio <- -1.5
ENDGLOBAL
