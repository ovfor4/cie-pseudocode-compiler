% CPSC-KEYWORD-CLASS(7)

# NAME

cpsc-keyword-class, cpsc-keyword-public, cpsc-keyword-private, cpsc-keyword-endclass, cpsc-keyword-new - define a class with fields and methods

# SYNOPSIS

```text
CLASS name
    PUBLIC publicField : type
    PRIVATE privateField : type
    PUBLIC PROCEDURE NEW(parameters)
        statements
    ENDPROCEDURE
    PUBLIC PROCEDURE method(parameters)
        statements
    ENDPROCEDURE
    FUNCTION getter(parameters) RETURNS type
        RETURN expression
    ENDFUNCTION
ENDCLASS
DECLARE object : ClassName(arguments)
object <- NEW ClassName(arguments)
```

# DESCRIPTION

Declare a class at top level with at least one field or method. Fields and methods are public by default; PUBLIC or PRIVATE may be specified for each member. Fields do not use DECLARE. ENDCLASS closes the declaration. Fields and methods share a case-insensitive member namespace; fields and methods cannot share a name. Methods with distinct parameter signatures may share a name when every definition is prefixed with OVERLOAD, after any access keyword; see [cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md).

Call methods as `Object.Method(arguments)` in expressions or as standalone statements, or use `CALL Object.Method(arguments)`. A receiver may be a variable, array element, nested field, or pointer dereference. Class-valued function and method results also support method calls, such as `Make().Get()`, using a temporary receiver. Direct field access, indexing and dereferencing on call results remain unsupported. Calls may precede definitions; methods may call later methods or recurse. Parameters and return values follow the ordinary FUNCTION and PROCEDURE rules.

Within a method, bare field names refer to the current object's fields. The compiler extension `this` refers directly to the current object: use `this.Field` or `this.Method(arguments)` without pointer dereferencing. An unqualified call such as `Method(arguments)` still names a global function or procedure. PRIVATE members are accessible only from methods of the owning class, including access through another instance of that class.

Parameters and local variables, arrays, or constants may shadow fields; this produces a warning with the source line and does not prevent compilation. The local name takes precedence; `this.Field` continues to access the field. The name `this` cannot be a class member or a method parameter or local declaration.

Classes retain record value semantics: zero initialization, whole-value copying, arrays, pointers, and ordinary function/procedure parameters. Methods modify the receiver's storage. The object reference `this` may be read, returned, or passed as an object, including BYREF to a procedure; `this <- Other` assigns into the current object without rebinding the reference. PRIVATE fields still initialize and copy with the whole value.

A constructor is a PROCEDURE named NEW, following guide section 10.2. It may be PUBLIC or PRIVATE; FUNCTION NEW and global PROCEDURE NEW are errors. Member names are case insensitive, so New and new also name the constructor. Multiple NEW procedures are allowed when every definition specifies OVERLOAD and their parameter signatures differ. Explicit `Object.NEW(arguments)`, `CALL Object.NEW(arguments)` and `this.NEW(arguments)` behave like ordinary member calls and may be repeated; they do not zero the object or reconstruct its fields.

`DECLARE Object : ClassName` zeroes the object's storage and constructs its fields recursively in declaration order; it then calls NEW only when the class has a parameterless NEW. Otherwise the object stays unconstructed until `Object <- NEW ClassName(arguments)` binds it, as in guide section 10.2. `DECLARE Object : ClassName(arguments)` always calls the matching NEW, so `ClassName()` is an error for a class whose NEW requires arguments, and supplying arguments to a class without NEW is an error. These automatic DECLARE, field and array calls are compiler extensions. Array elements and class-valued fields in records or classes follow the plain DECLARE rule, before the enclosing constructor runs. Constructor arguments after ARRAY OF are unsupported. A declaration executed again, such as inside a loop, first destroys its previous live incarnation, then zeroes and constructs its original storage again.

An `OVERLOAD PROCEDURE NEW(BYREF Other : ClassName)` defines a copy constructor. BYVAL class lvalue arguments and RETURN of class lvalues invoke it; a same-class BYVAL constructor is rejected. Classes without one retain bitwise copying for fields without STRING ownership. Records and BYVAL arrays recursively clone STRING-bearing fields or elements. Other owning class fields retain the existing shallow-copy limitations. Class results transfer ownership directly into BYVAL arguments, RETURN and ordinary assignment without an assignment overload. Discarded results and temporary receivers live until the statement ends, including exception exits. Ordinary user BYREF calls still require bare variables; constructor calls can bind temporaries.

The keyword NEW is allowed only as the complete right side of assignment: `Target <- NEW ClassName`, `Target <- NEW ClassName()` or `Target <- NEW ClassName(arguments)`. Target must have exactly the named class type. NEW uses the checked MALLOC allocation path, zeroes storage and constructs fields before calling NEW. A method-only class receives at least one byte of heap storage so its object address is non-null. Allocation failure reports a runtime error and throws; constructor exceptions also propagate to TRY/CATCH. Private constructor access and argument checks follow ordinary member-call rules.

When Target is a declared class variable, including a function local or BYVAL parameter, NEW rebinds that variable to the heap object. No `^` dereference is needed to access it. Address-of, BYREF calls and method calls use its current object. Pointers taken before rebinding continue to refer to the previous object. Ordinary `A <- B` assigns into A's current object without rebinding A: it calls a matching member OPERATOR<- when defined, otherwise it copies the same-class value. See [cpsc-keyword-operator(7)](cpsc-keyword-operator.7.md). BYVAL parameters own their copied or transferred value and can rebind independently.

For an array element, field (including a bare field inside a method), BYREF parameter, this, or pointer dereference, NEW constructs a heap object and copies its value into the existing target. That temporary allocation is retained. INHERITS, SUPER and array fields remain unsupported.

A parameterless PROCEDURE DELETE defines a destructor. Explicit DELETE method calls are ordinary calls; keyword DELETE validates a heap object, destroys it and its fields, then frees the allocation. DECLARE storage and BYVAL copies are automatically destroyed in reverse order at normal function exit and during unwinding; main-level objects are destroyed only at normal program end. NEW rebinding leaves automatic destruction attached to the original storage. See [cpsc-keyword-delete(7)](cpsc-keyword-delete.7.md) for lifetime and access rules.

# EXAMPLE

```text
CLASS Counter
    PRIVATE Value : INTEGER
    PUBLIC PROCEDURE NEW(Number : INTEGER)
        this.Value <- Number
    ENDPROCEDURE
    FUNCTION Get() RETURNS INTEGER
        RETURN Value
    ENDFUNCTION
ENDCLASS
DECLARE Item : Counter(3)
Item <- NEW Counter(5)
OUTPUT Item.Get()
```

# SEE ALSO

[cpsc-keyword-template(7)](cpsc-keyword-template.7.md), [cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md), [cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-delete(7)](cpsc-keyword-delete.7.md), [cpsc-keyword-type(7)](cpsc-keyword-type.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md), [cpsc-keyword-procedure(7)](cpsc-keyword-procedure.7.md), [cpsc-keyword-call(7)](cpsc-keyword-call.7.md).
