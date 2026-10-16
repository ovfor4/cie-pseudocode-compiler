CONSTANT Rate = 7
DECLARE X : INTEGER
DECLARE Flag : BOOLEAN
DECLARE Letter : CHAR
DECLARE Text : STRING
X <- Rate + 3 * 4
Flag <- (X = 19) AND NOT FALSE
Letter <- 'Q'
Text <- "ready"
OUTPUT Text, ":", Letter, ":", X, ":", Flag
OUTPUT (X - 4) DIV 3, ":", (X - 4) MOD 4
OUTPUT 2 < 3, ":", 2 <= 2, ":", 3 > 2, ":", 3 >= 3, ":", 2 <> 3
