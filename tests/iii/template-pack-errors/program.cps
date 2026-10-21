TEMPLATE <Rest...>
PROCEDURE NotLast(Values : Rest..., Extra : INTEGER)
ENDPROCEDURE

TEMPLATE <Rest...>
PROCEDURE ArrayPack(Values : ARRAY OF Rest...)
ENDPROCEDURE

TEMPLATE <T>
PROCEDURE WrongPack(Values : T...)
ENDPROCEDURE

TEMPLATE <Rest...>
PROCEDURE BarePack(Values : Rest...)
    OUTPUT Values
ENDPROCEDURE

TEMPLATE <T>
PROCEDURE ScalarExpansion(Value : T)
    CALL Missing(Value...)
ENDPROCEDURE

TEMPLATE <T>
PROCEDURE WrongTypeExpansion(Value : T)
    CALL Missing<T...>(Value)
ENDPROCEDURE

TEMPLATE <Rest...>
PROCEDURE MissingDots(Values : Rest)
ENDPROCEDURE

OUTPUT 0
