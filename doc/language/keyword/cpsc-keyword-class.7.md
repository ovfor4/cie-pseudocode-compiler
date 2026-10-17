% CPSC-KEYWORD-CLASS(7)

# NAME

cpsc-keyword-class, cpsc-keyword-public, cpsc-keyword-private, cpsc-keyword-endclass - define a class with fields and methods

# SYNOPSIS

```text
CLASS name
    PUBLIC publicField : type
    PRIVATE privateField : type
    PUBLIC PROCEDURE method(parameters)
        statements
    ENDPROCEDURE
    FUNCTION getter(parameters) RETURNS type
        RETURN expression
    ENDFUNCTION
ENDCLASS
```

# DESCRIPTION

Declare a class at top level with at least one field or method. Fields and methods are public by default; PUBLIC or PRIVATE may be specified for each member. Fields do not use DECLARE. ENDCLASS closes the declaration. Fields and methods share a case-insensitive member namespace; duplicate names and method overloading are rejected.

Call methods as `Object.Method(arguments)` in expressions or as standalone statements, or use `CALL Object.Method(arguments)`. A receiver may be a variable, array element, nested field, or pointer dereference. Calls may precede definitions; methods may call later methods or recurse. Parameters and return values follow the ordinary FUNCTION and PROCEDURE rules. Access on function or method results remains unsupported.

Within a method, bare field names refer to the current object's fields. The compiler extension `this` refers directly to the current object: use `this.Field` or `this.Method(arguments)` without pointer dereferencing. An unqualified call such as `Method(arguments)` still names a global function or procedure. PRIVATE members are accessible only from methods of the owning class, including access through another instance of that class.

Parameters and local variables, arrays, or constants may shadow fields; this produces a warning with the source line and does not prevent compilation. The local name takes precedence; `this.Field` continues to access the field. The name `this` cannot be a class member or a method parameter or local declaration.

Classes retain record value semantics: zero initialization, whole-value copying, arrays, pointers, and ordinary function/procedure parameters. Methods modify the receiver's storage. The object reference `this` may be read, returned, or passed as an object, including BYREF to a procedure; `this <- Other` copies Other into the current object without rebinding the reference. PRIVATE fields still initialize and copy with the whole value. Constructors, destructors, NEW, INHERITS, and array fields are unsupported.

# EXAMPLE

```text
CLASS Counter
    PRIVATE Value : INTEGER
    PUBLIC PROCEDURE Set(Number : INTEGER)
        this.Value <- Number
    ENDPROCEDURE
    FUNCTION Get() RETURNS INTEGER
        RETURN Value
    ENDFUNCTION
ENDCLASS
DECLARE Item : Counter
Item.Set(3)
OUTPUT Item.Get()
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-type(7)](cpsc-keyword-type.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md), [cpsc-keyword-procedure(7)](cpsc-keyword-procedure.7.md), [cpsc-keyword-call(7)](cpsc-keyword-call.7.md).
