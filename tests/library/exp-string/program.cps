INCLUDE "../../../pseudocode-lib/string.cps"
FUNCTION Echo(S : STRING) RETURNS STRING
    RETURN S
ENDFUNCTION
DECLARE A : STRING((C_STRING)"hello")
DECLARE B : STRING(A)
B <- (C_STRING)"world"
B <- B
OUTPUT (A & B)
DECLARE Empty : STRING
OUTPUT LENGTH(Empty)
DECLARE Nuls : STRING(3, (C_CHAR)'\0')
OUTPUT LENGTH(Nuls)
DECLARE Long : STRING(20000, (C_CHAR)'x')
DECLARE I : INTEGER
FOR I <- 1 TO 100
    Long <- Long & A
NEXT I
OUTPUT LENGTH(Long)
Long <- Echo(Long)
OUTPUT LENGTH(Long)
OUTPUT (NUM_TO_STR(42) & NUM_TO_STR(1.25))
DECLARE Number : STRING((C_STRING)"12.5")
OUTPUT STR_TO_NUM(Number) = 12.5
TRY
    Number <- (C_STRING)"12x"
    OUTPUT STR_TO_NUM(Number)
CATCH
    OUTPUT "conversion caught"
ENDTRY
TRY
    DECLARE Invalid : STRING(-1, (C_CHAR)'x')
CATCH
    OUTPUT "length caught"
ENDTRY
CALL NUM_TO_STR(99)
CALL Echo(Long)
DECLARE Heap : STRING
Heap <- NEW STRING((C_STRING)"heap")
OUTPUT (Heap)
DELETE Heap
CALL CheckLeft()
CALL CheckRight()
CALL CheckMid()
CALL CheckLCase()
CALL CheckUCase()
CALL CheckAsc()
CALL CheckChr()
CALL CheckIsNum()

PROCEDURE CheckLeft()
    DECLARE Letters : STRING((C_STRING)"ABCDEFGH")
    DECLARE Empty : STRING
    DECLARE Nuls : STRING(3, (C_CHAR)'\0')
    DECLARE A : STRING((C_STRING)"hello")
    OUTPUT (LEFT(Letters, 3))
    OUTPUT LENGTH(LEFT(Empty, 2)), ":", LENGTH(LEFT(Letters, 0)), ":", LENGTH(LEFT(Letters, -1))
    OUTPUT (LEFT(Letters, 20))
    OUTPUT LENGTH(LEFT(Nuls & A, 4))
ENDPROCEDURE

PROCEDURE CheckRight()
    DECLARE Letters : STRING((C_STRING)"ABCDEFGH")
    DECLARE Empty : STRING
    DECLARE Nuls : STRING(3, (C_CHAR)'\0')
    DECLARE A : STRING((C_STRING)"hello")
    OUTPUT (RIGHT(Letters, 3))
    OUTPUT LENGTH(RIGHT(Empty, 2)), ":", LENGTH(RIGHT(Letters, 0)), ":", LENGTH(RIGHT(Letters, -1))
    OUTPUT (RIGHT(Letters, 20))
    OUTPUT LENGTH(RIGHT(A & Nuls, 4))
ENDPROCEDURE

PROCEDURE CheckMid()
    DECLARE Letters : STRING((C_STRING)"ABCDEFGH")
    DECLARE Empty : STRING
    DECLARE Nuls : STRING(3, (C_CHAR)'\0')
    OUTPUT (MID(Letters, 2, 3))
    OUTPUT (MID(Letters, -9223372036854775808, 2))
    OUTPUT (MID(Letters, 7, 9223372036854775807))
    OUTPUT LENGTH(MID(Empty, 1, 2)), ":", LENGTH(MID(Letters, 9223372036854775807, 2))
    OUTPUT LENGTH(MID(Letters, 2, 0)), ":", LENGTH(MID(Letters, 2, -1))
    OUTPUT LENGTH(MID(Nuls & Letters, 2, 4))
ENDPROCEDURE

PROCEDURE CheckLCase()
    DECLARE Mixed : STRING((C_STRING)"AbC@19")
    DECLARE Empty : STRING
    DECLARE Nuls : STRING(2, (C_CHAR)'\0')
    OUTPUT (LCASE(Mixed))
    OUTPUT (Mixed)
    DECLARE Lower : CHAR
    Lower <- LCASE('W')
    OUTPUT Lower, ":", LCASE('w'), ":", LCASE('@')
    OUTPUT LENGTH(LCASE(Empty)), ":", LENGTH(LCASE(Nuls & Mixed))
    OUTPUT (MID(LCASE(Nuls & Mixed), 3, 3))
ENDPROCEDURE

PROCEDURE CheckUCase()
    DECLARE Mixed : STRING((C_STRING)"AbC@19")
    DECLARE Empty : STRING
    DECLARE Nuls : STRING(2, (C_CHAR)'\0')
    OUTPUT (UCASE(Mixed))
    OUTPUT (Mixed)
    DECLARE Upper : CHAR
    Upper <- UCASE('h')
    OUTPUT Upper, ":", UCASE('H'), ":", UCASE('@')
    OUTPUT LENGTH(UCASE(Empty)), ":", LENGTH(UCASE(Nuls & Mixed))
    OUTPUT (MID(UCASE(Nuls & Mixed), 3, 3))
ENDPROCEDURE

PROCEDURE CheckAsc()
    DECLARE Letters : STRING((C_STRING)"ABCDEFGH")
    DECLARE Empty : STRING
    DECLARE Nuls : STRING(2, (C_CHAR)'\0')
    DECLARE High : STRING(1, (C_CHAR)255)
    OUTPUT ASC('A'), ":", ASC(Letters)
    OUTPUT ASC(Empty), ":", ASC(Nuls), ":", ASC('\0')
    OUTPUT ASC(High)
ENDPROCEDURE

PROCEDURE CheckChr()
    OUTPUT (CHR(65) & CHR(90))
    OUTPUT ASC(CHR(ASC('A')))
    OUTPUT LENGTH(CHR(0)), ":", ASC(CHR(0))
    OUTPUT ASC(CHR(321)), ":", ASC(CHR(-1))
ENDPROCEDURE

PROCEDURE CheckIsNum()
    DECLARE Number : STRING((C_STRING)"12.5")
    DECLARE Junk : STRING((C_STRING)"12x")
    DECLARE Empty : STRING
    DECLARE Leading : STRING((C_STRING)" 12.5")
    DECLARE Trailing : STRING((C_STRING)"12.5 ")
    DECLARE Whitespace : STRING((C_STRING)" ")
    OUTPUT IS_NUM(Number), ":", IS_NUM(Junk), ":", IS_NUM(Empty)
    OUTPUT IS_NUM(Leading), ":", IS_NUM(Trailing), ":", IS_NUM(Whitespace)
    OUTPUT IS_NUM(Number & CHR(0)), ":", IS_NUM(CHR(0) & Number)
ENDPROCEDURE
