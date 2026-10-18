% CPSC-KEYWORD-OPERATOR(7)

# NAME

cpsc-keyword-operator - overload unary and binary operators

# SYNOPSIS

```text
OVERLOAD FUNCTION OPERATOR+(left : T, right : U) RETURNS R
    statements
ENDFUNCTION
CLASS T
    OVERLOAD FUNCTION OPERATOR+(right : U) RETURNS R
        statements
    ENDFUNCTION
    OVERLOAD FUNCTION OPERATOR-() RETURNS R
        statements
    ENDFUNCTION
ENDCLASS
```

# DESCRIPTION

OPERATOR is a compiler extension, absent from the CIE guide. Declare operators with OVERLOAD FUNCTION, at top level or inside CLASS. OPERATOR is reserved; whitespace before the operator is optional. Parentheses are required. Parameters and return values follow FUNCTION rules: parameters default to BYVAL, BYREF is forbidden, and omitting RETURNS defaults to INTEGER. Whole-array operands and array results are unsupported.

Binary operators are `+ - * / DIV MOD & = <> < <= > >= AND OR`. Unary operators are `- NOT`. A global binary definition takes two parameters and a unary definition takes one; at least one parameter must be a record, class or enum. A pointer alias alone does not qualify. A member binary definition takes one explicit parameter and a unary definition takes none; this supplies the left or sole operand. PUBLIC and PRIVATE precede OVERLOAD as for other methods.

Operands match exact static types, without implicit conversions, following OVERLOAD rules. For example, an INTEGER operand does not match a REAL parameter. Visible global definitions and methods of the left operand's class participate together; multiple matching candidates are an error. The selected method must be accessible: PRIVATE never causes a public fallback. Included units provide operators using the ordinary INCLUDE visibility rules. Return types do not distinguish overloads.

The expression has the selected function's declared return type. Comparisons and logic may return any supported function result type: a comparison returning STRING can be printed or assigned to STRING, but cannot be implicitly used as BOOLEAN. CASE comparisons must return BOOLEAN. A VOID result may be discarded through an explicit call but cannot be used as a value.

Operators keep their existing precedence, operand count and associativity. Operands are evaluated once, from left to right; overloaded AND and OR evaluate both operands. Operators are independent: defining = does not define <>, and operands are never swapped to find a match. A matching overload takes precedence over the built-in operation; otherwise the existing built-in rules apply.

Member operators on variables, fields, array elements and pointer dereferences modify the original receiver. Class-valued expressions can also receive operator calls, supporting chains such as `(A + B) + C`. A temporary receiver follows the compiler's existing function-scoped object cleanup rules, rather than C++ full-expression lifetimes. Copies and return values retain the ordinary class value semantics.

Explicit calls use `OPERATOR+(A, B)` for global definitions and `A.OPERATOR+(B)` for members. They also work as statements, optionally prefixed with CALL, to discard results. As with ordinary method calls, explicit member access on a function result is unsupported. Assignment, indexing, function-call, address-of and dereference operators cannot be overloaded.

# EXAMPLE

```text
CLASS Box
    Value : INTEGER
    OVERLOAD FUNCTION OPERATOR+(Right : INTEGER) RETURNS Box
        DECLARE Result : Box
        Result.Value <- Value + Right
        RETURN Result
    ENDFUNCTION
ENDCLASS
DECLARE Item : Box
DECLARE Sum : Box
Item.Value <- 3
Sum <- (Item + 2) + 4
OUTPUT Sum.Value
```

# SEE ALSO

[cpsc-keyword-overload(7)](cpsc-keyword-overload.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md), [cpsc-keyword-class(7)](cpsc-keyword-class.7.md), [cpsc-keyword-include(7)](cpsc-keyword-include.7.md).
