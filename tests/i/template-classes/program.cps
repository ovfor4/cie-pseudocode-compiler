TEMPLATE <T>
CLASS Box
    PRIVATE Value : T
    PUBLIC OVERLOAD PROCEDURE NEW()
    ENDPROCEDURE
    PUBLIC OVERLOAD PROCEDURE NEW(V : T)
        Value <- V
    ENDPROCEDURE
    PUBLIC FUNCTION Get() RETURNS T
        RETURN Value
    ENDFUNCTION
    PUBLIC PROCEDURE Set(V : T)
        Value <- V
    ENDPROCEDURE
ENDCLASS

TEMPLATE <T>
FUNCTION Make() RETURNS T
    DECLARE Value : T
    RETURN Value
ENDFUNCTION

TEMPLATE <A, B>
CLASS Pair
    First : A
    Second : B
    PROCEDURE NEW(Left : A, Right : B)
        First <- Left
        Second <- Right
    ENDPROCEDURE
ENDCLASS

TEMPLATE <T>
CLASS Stack3
    PRIVATE First : T
    PRIVATE Second : T
    PRIVATE Third : T
    PRIVATE Count : INTEGER
    PROCEDURE Push(Value : T)
        Count <- Count + 1
        CASE OF Count
            1 : First <- Value
            2 : Second <- Value
            3 : Third <- Value
        ENDCASE
    ENDPROCEDURE
    FUNCTION Pop() RETURNS T
        Count <- Count - 1
        CASE OF Count
            2 : RETURN Third
            1 : RETURN Second
            OTHERWISE : RETURN First
        ENDCASE
    ENDFUNCTION
ENDCLASS

OVERLOAD PROCEDURE PrintAll()
ENDPROCEDURE
TEMPLATE <T, Rest...>
OVERLOAD PROCEDURE PrintAll(First : T, Others : Rest...)
    OUTPUT First
    CALL PrintAll(Others...)
ENDPROCEDURE

TEMPLATE <Types...>
CLASS Group
    PROCEDURE Show(Values : Types...)
        CALL PrintAll(Values...)
    ENDPROCEDURE
ENDCLASS
TEMPLATE <Types...>
PROCEDURE ShowGroup(Values : Types...)
    DECLARE Items : Group<Types...>
    CALL Items.Show(Values...)
ENDPROCEDURE

// Instantiation during the type pre-pass defers the Box method prototypes.
TYPE Holder
    DECLARE Item : Box<INTEGER>
ENDTYPE
OUTPUT Make<Box<BOOLEAN>>().Get()
DECLARE H : Holder
OUTPUT H.Item.Get()
DECLARE B : Box<INTEGER>(7)
OUTPUT B.Get()
DECLARE P : Pair<INTEGER, REAL>(3, 2.5)
OUTPUT P.First
OUTPUT P.Second
DECLARE S : Stack3<INTEGER>
CALL S.Push(10)
CALL S.Push(20)
CALL S.Push(30)
OUTPUT S.Pop()
OUTPUT S.Pop()
OUTPUT S.Pop()
B <- NEW Box<INTEGER>(5)
OUTPUT B.Get()
DELETE B
DECLARE Inner : Box<INTEGER>(9)
DECLARE Outer : Box<Box<INTEGER>>(Inner)
OUTPUT Outer.Get().Get()
DECLARE Text : Box<STRING>("alpha")
DECLARE Copy : Box<STRING>
Copy <- Text
CALL Text.Set("beta")
OUTPUT Copy.Get()
OUTPUT Text.Get()
DECLARE Values : Group<INTEGER, REAL, CHAR>
CALL Values.Show(1, 2.5, 'c')
CALL ShowGroup()
