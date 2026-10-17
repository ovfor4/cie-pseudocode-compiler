CLASS Counter
    PRIVATE Value : INTEGER
    PROCEDURE Set(Number : INTEGER)
        Value <- Number
    ENDPROCEDURE
    FUNCTION Get() RETURNS INTEGER
        RETURN Value
    ENDFUNCTION
    PROCEDURE Add(Number : INTEGER)
        this.Set(this.Get() + Number)
    ENDPROCEDURE
    PROCEDURE Copy(Other : Counter)
        this <- Other
    ENDPROCEDURE
    FUNCTION Clone() RETURNS Counter
        RETURN this
    ENDFUNCTION
    FUNCTION ReadOther(Other : Counter) RETURNS INTEGER
        RETURN Other.Hidden()
    ENDFUNCTION
    PRIVATE FUNCTION Hidden() RETURNS INTEGER
        RETURN Value
    ENDFUNCTION
    FUNCTION Sum(Number : INTEGER) RETURNS INTEGER
        IF Number = 0 THEN
            RETURN 0
        ENDIF
        RETURN Number + this.Sum(Number - 1)
    ENDFUNCTION
    PROCEDURE Take(BYREF Number : INTEGER, BYREF Values : ARRAY OF INTEGER)
        Number <- Number + Value
        Values[1] <- Number
    ENDPROCEDURE
    PROCEDURE Fail()
        THROW
    ENDPROCEDURE
    PROCEDURE PassSelf()
        CALL Change(this)
    ENDPROCEDURE
ENDCLASS
CLASS Marker
    FUNCTION Get() RETURNS INTEGER
        RETURN 99
    ENDFUNCTION
ENDCLASS
TYPE Container
    DECLARE Item : Counter
ENDTYPE
TYPE CounterPointer = ^Counter
PROCEDURE Change(BYREF Target : Counter)
    Target.Set(42)
ENDPROCEDURE
FUNCTION Get() RETURNS INTEGER
    RETURN 100
ENDFUNCTION
DECLARE A : Counter
DECLARE B : Counter
CALL a.sET(3)
B.Set(8)
A.Add(2)
OUTPUT A.Get(), ",", B.Get()
CALL A.Copy(B)
A.Set(9)
OUTPUT A.Get(), ",", B.Get()
B <- A.Clone()
OUTPUT B.Get(), ",", A.ReadOther(B)
OUTPUT A.Sum(4), ",", Get()
DECLARE Items : ARRAY[1:2] OF Counter
Items[1].Set(6)
DECLARE Wrapped : Container
Wrapped.Item.Set(7)
DECLARE P : CounterPointer
P <- ^Items[1]
P^.Add(4)
OUTPUT Items[1].Get(), ",", Wrapped.Item.Get()
DECLARE Number : INTEGER
DECLARE Values : ARRAY[1:2] OF INTEGER
Number <- 1
CALL A.Take(Number, Values)
OUTPUT Number, ",", Values[1]
TRY
    A.Fail()
CATCH
    OUTPUT "caught"
ENDTRY
A.PassSelf()
OUTPUT A.Get()
DECLARE M : Marker
OUTPUT M.Get()
