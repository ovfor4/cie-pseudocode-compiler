TEMPLATE <T>
FUNCTION Identity(Value : T) RETURNS T
    IF Value < Value THEN
        RETURN (T)Value
    ENDIF
    RETURN Value
ENDFUNCTION

TEMPLATE <T, Rest...>
OVERLOAD PROCEDURE Visit(First : T, Others : Rest...)
    OUTPUT Identity<T>(First)
    CALL Visit<Rest...>(Others...)
    Visit<Box<Pair<INTEGER,REAL>>>(Others...)
ENDPROCEDURE

TEMPLATE <T>
CLASS Box
    PRIVATE Value : T
    PUBLIC PROCEDURE NEW(V : T)
        Value <- V
    ENDPROCEDURE
    PUBLIC FUNCTION Get() RETURNS T
        RETURN Value
    ENDFUNCTION
    PUBLIC FUNCTION AsBox(V : Box<T>) RETURNS Box<T>
        RETURN (Box<T>)V
    ENDFUNCTION
ENDCLASS

TEMPLATE <A, B>
CLASS Pair
    Left : A
    Right : B
ENDCLASS

TEMPLATE <T, Rest...>
CLASS Bundle
    PUBLIC PROCEDURE Send(Values : Rest...)
        CALL Visit(Values...)
    ENDPROCEDURE
ENDCLASS

TEMPLATE <T>
PROCEDURE NestedTypes(Value : Box<Box<T>>)
    DECLARE Other : Box<Pair<T,REAL>>
    Other <- NEW Box<Pair<T,REAL>>()
    Value <- (Box<Box<T>>)Value
ENDPROCEDURE
