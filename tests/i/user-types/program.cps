TYPE Point
    DECLARE X : INTEGER
    DECLARE Name : STRING
ENDTYPE
TYPE PointPtr = ^Point
TYPE Season = (Spring, Summer, Autumn, Winter)
CLASS Box
    PUBLIC Value : INTEGER
    PRIVATE Hidden : INTEGER
ENDCLASS
DECLARE A : Point
DECLARE B : Point
DECLARE P : PointPtr
DECLARE S : Season
DECLARE First : Box
DECLARE Second : Box
A.X <- 7
A.Name <- "first"
B <- A
P <- ^A
P^.X <- 9
S <- Spring
S <- S + 2
First.Value <- 4
Second <- First
Second.Value <- 8
OUTPUT A.X, ":", B.X, ":", B.Name, ":", S = Autumn
OUTPUT First.Value, ":", Second.Value
