% CPSC-KEYWORD-TEMPLATE(7)

# NAME

cpsc-keyword-template - define function, procedure and class templates

# SYNOPSIS

```text
TEMPLATE <T, Rest...>
[OVERLOAD] FUNCTION name(parameters) RETURNS type
    statements
ENDFUNCTION
TEMPLATE <T, Rest...>
[OVERLOAD] PROCEDURE name(parameters)
    statements
ENDPROCEDURE
TEMPLATE <T, Rest...>
CLASS name
    fields and methods
ENDCLASS
```

# DESCRIPTION

TEMPLATE is a compiler extension, absent from the CIE guide. Place it before a top-level FUNCTION, PROCEDURE or CLASS, before OVERLOAD when present. Every template parameter names a type; no typename keyword is needed. Names must be distinct. A single optional type pack, written `Rest...`, must come last. Each concrete use creates and caches an ordinary function or class instance by substituting types into the template body.

Function calls deduce types from parameters typed `T` or `ARRAY OF T`; bounded array parameters also require matching rank. Deduction does not look inside `Box<T>`. Explicit arguments, as in `Max<REAL>(1.5, 2.0)`, bind types positionally before deduction fills any remaining parameters. All argument types must match exactly, including explicitly bound types: `Max<REAL>(1, 2.5)` is an error. Explicit arguments are forbidden on ordinary functions and builtins. A type used only in RETURNS requires an explicit argument.

A final parameter `Others : Rest...` expands into one parameter per pack type, possibly none. It cannot be an ARRAY parameter. Write `Others...` only in argument lists of function or method calls, NEW or DECLARE constructors. A bare pack value is invalid. In type argument lists, `Rest...` forwards the current type pack. Explicit pack arguments constrain a prefix; deduction can supply the remaining types.

When multiple function or procedure definitions share a name, every definition, including ordinary definitions, must specify OVERLOAD. Selection prefers an exact ordinary definition, then a non-variadic template, then a variadic template. Two matches at the same level are ambiguous; no further template ordering is performed. Explicit template arguments select only templates. Normal BYREF, array-bound and access checks apply after selection; see [cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md).

Use a class instance such as `Box<INTEGER>` wherever a type name is accepted, including nested instances such as `Box<Box<INTEGER>>`. Supply all class type arguments explicitly; variadic CLASS templates are allowed, and their methods may use the class's pack parameters. Pack fields such as `Values : Rest...` are rejected. Constructors, ownership and destruction follow ordinary CLASS rules. In particular, a declaration without constructor arguments needs a matching parameterless NEW when constructors exist; see [cpsc-keyword-class(7)](cpsc-keyword-class.7.md).

Non-type parameters, SIZEOF..., member templates, operator templates, specialization and pointer-alias templates are unsupported. Within a class template, write the full instance type rather than the bare class name. Pointer-linked generic containers remain unsupported because pointer-alias templates are unavailable.

Source template lists cannot be empty: `<>` is the inequality token. Nested closing `>>` works as two tokens. Separate `<` from a following `-` and `>` from a following `=` to avoid the `<-` and `>=` tokens. Expression lookahead treats `A < B, C > (D)` as a template call; parenthesize comparisons to avoid this ambiguity.

# EXAMPLE

```text
TEMPLATE <T>
FUNCTION Max(A : T, B : T) RETURNS T
    IF A > B THEN
        RETURN A
    ENDIF
    RETURN B
ENDFUNCTION

OVERLOAD PROCEDURE PrintAll()
ENDPROCEDURE
TEMPLATE <T, Rest...>
OVERLOAD PROCEDURE PrintAll(First : T, Others : Rest...)
    OUTPUT First
    CALL PrintAll(Others...)
ENDPROCEDURE

TEMPLATE <T>
CLASS Box
    Value : T
    OVERLOAD PROCEDURE NEW()
    ENDPROCEDURE
    OVERLOAD PROCEDURE NEW(V : T)
        Value <- V
    ENDPROCEDURE
ENDCLASS

DECLARE B : Box<INTEGER>
B.Value <- Max(3, 7)
OUTPUT Max<REAL>(1.5, 2.0)
CALL PrintAll(B.Value, 2.5, 'c', "str")
```

# SEE ALSO

[cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md), [cpsc-keyword-class(7)](cpsc-keyword-class.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md), [cpsc-keyword-procedure(7)](cpsc-keyword-procedure.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md), [cpsc-language-overview(7)](../cpsc-language-overview.7.md).
