TEMPLATE <T, Rest...>
PROCEDURE MissingBase(First : T, Others : Rest...)
    CALL MissingBase(Others...)
ENDPROCEDURE

TEMPLATE <Types...>
PROCEDURE BadOutput(Values : Types...)
    OUTPUT Values...
ENDPROCEDURE

TEMPLATE <Types...>
PROCEDURE BadIndex(Values : Types...)
    DECLARE Items : ARRAY[1:2] OF INTEGER
    OUTPUT Items[Values...]
ENDPROCEDURE

TEMPLATE <Types...>
PROCEDURE BadOperand(Values : Types...)
    OUTPUT 1 + Values...
ENDPROCEDURE

TEMPLATE <T, Rest...>
PROCEDURE Exact(First : T, Others : Rest...)
ENDPROCEDURE

TEMPLATE <A...>
OVERLOAD PROCEDURE Tied(Values : A...)
ENDPROCEDURE
TEMPLATE <B...>
OVERLOAD PROCEDURE Tied(Values : B...)
ENDPROCEDURE

CALL MissingBase(1)
CALL BadOutput(1)
CALL BadIndex(1)
CALL BadOperand(1)
CALL Exact<INTEGER, REAL>(1, 2)
CALL Exact<INTEGER, REAL, CHAR>(1, 2.5)
CALL Tied(1)
