TYPE NativeLong = C_TYPE("long")
TYPE NativeChar = C_TYPE("char")
TYPE Byte = C_TYPE("unsigned char")
TYPE Word = C_TYPE("unsigned int")
DECLARE Count : INTEGER
DECLARE Index : INTEGER
DECLARE A : INTEGER
DECLARE B : INTEGER
DECLARE L : NativeLong
DECLARE C : NativeChar
DECLARE U : Byte
DECLARE W : Word
INPUT Count
FOR Index <- 1 TO Count
    INPUT A
    INPUT B
    L <- (NativeLong) A
    C <- (NativeChar) B
    U <- (Byte) B
    W <- (Word) A
    OUTPUT L, ":", (INTEGER) C, ":", U + (Byte) 1, ":", W + (Word) 1
NEXT Index
