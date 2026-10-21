// Open in a dark theme; keywords are uppercase and comments ignore [(
TYPE Season = (Spring, Summer, Autumn, Winter)
TYPE IntPointer = ^INTEGER
TYPE Student
	DECLARE Name : STRING
	DECLARE Initial : CHAR
ENDTYPE

CONSTANT Limit = 3

DECLARE Values : ARRAY[1:3] OF INTEGER
DECLARE Index : INTEGER
DECLARE Total : REAL
DECLARE Ready : BOOLEAN
DECLARE Text : STRING
DECLARE Letter : CHAR
Ready <- TRUE AND NOT FALSE
Total <- -3.5 + 2 * 4 / 2
Text <- "CIE // [()]" & " pseudocode"
Letter <- '\''
Text <- "Quote: \"; slash: \\; byte: \x41; Unicode: \u0042"
DECLARE RawText : C_STRING
RawText <- "Native text"C

FOR Index <- 1 TO 3 STEP 1
	Values[Index] <- Index MOD 2
NEXT Index
IF Ready OR Total >= 1 THEN
	OUTPUT MID(Text, 1, LENGTH(Text))
ELSE
	OUTPUT "Try again"
ENDIF

FUNCTION Double(Value : INTEGER) RETURNS INTEGER
	RETURN Value * 2
ENDFUNCTION
PROCEDURE Swap(BYREF X : INTEGER, Y : INTEGER)
	DECLARE Temp : INTEGER
	Temp <- X
	X <- Y
	Y <- Temp
ENDPROCEDURE

OPENFILE "example.txt" FOR WRITE
WRITEFILE "example.txt", Text
CLOSEFILE "example.txt"

TRY
	THROW
CATCH
	OUTPUT "Caught an exception"
ENDTRY

CASE OF Index
	1 : OUTPUT "First"
	2 TO Limit : OUTPUT "In range"
	OTHERWISE : OUTPUT "Other"
ENDCASE
