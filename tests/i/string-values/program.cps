GLOBAL
    DECLARE Label : STRING
    Label <- "global"
ENDGLOBAL
CONSTANT Greeting = "constant"
TYPE Item
    DECLARE Text : STRING
ENDTYPE
FUNCTION ReadConstant() RETURNS STRING
    CONSTANT Local = "local"
    RETURN Local
ENDFUNCTION
FUNCTION Echo(Value : Item) RETURNS Item
    RETURN Value
ENDFUNCTION
PROCEDURE Change(Values : ARRAY OF STRING)
    Values[1] <- "changed"
    OUTPUT Values[1], ":", Values[2]
ENDPROCEDURE
DECLARE A : Item
DECLARE B : Item
A.Text <- Greeting
B <- Echo(A)
B.Text <- "independent"
B <- B
OUTPUT A.Text, ":", B.Text, ":", Label, ":", ReadConstant()
DECLARE Values : ARRAY[1:2] OF STRING
Values[1] <- "one"
Values[2] <- "two"
CALL Change(Values)
OUTPUT Values
OUTPUT "A" < "B", ":", "B" > "A", ":", "A" <= "A", ":", "B" >= "A", ":", "a" <> "A"
OUTPUT LENGTH("a\0b"), ":", LENGTH(CHR(0))
DECLARE Selected : STRING
Selected <- "B"
CASE OF Selected
    "A" : OUTPUT "wrong"
    "B" : OUTPUT "matched"
ENDCASE
OUTPUT Selected
