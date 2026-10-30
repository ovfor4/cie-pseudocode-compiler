% CPSC-KEYWORD-OPERATOR(7)

# NAME

cpsc-keyword-operator - overload unary, binary and assignment operators

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
    OVERLOAD PROCEDURE OPERATOR<-(BYREF right : T)
        statements
    ENDPROCEDURE
ENDCLASS
```

# DESCRIPTION

OPERATOR is a compiler extension, absent from the CIE guide. Declare unary and binary operators with OVERLOAD FUNCTION, at top level or inside CLASS. Assignment uses a member OVERLOAD PROCEDURE as described below. OPERATOR is reserved; whitespace before the operator is optional. Parentheses are required. Operator functions follow FUNCTION rules: parameters default to BYVAL, BYREF is forbidden, and omitting RETURNS defaults to INTEGER. Whole-array operands and array results are unsupported.

Binary operators are `+ - * / DIV MOD & = <> < <= > >= AND OR`. Unary operators are `- NOT`. A global binary definition takes two parameters and a unary definition takes one; at least one parameter must be a record, class or enum. A pointer alias alone does not qualify. A member binary definition takes one explicit parameter and a unary definition takes none; this supplies the left or sole operand. PUBLIC and PRIVATE precede OVERLOAD as for other methods.

Operands prefer exact static types, following OVERLOAD rules. When no exact candidate or directly supported built-in operation applies, BYVAL operands may use IMPLICIT constructors; see [cpsc-keyword-implicit(7)](cpsc-keyword-implicit.7.md). For example, an INTEGER operand does not match a REAL parameter. Visible global definitions and methods of the left operand's class participate together. The selected method must be accessible: PRIVATE never causes a public fallback. Included units provide operators using the ordinary INCLUDE visibility rules.

Return types also distinguish operator overloads. When the context requires a result type (assignment, RETURN, condition, CASE comparison, subscript and similar positions), only definitions returning exactly that type match; if the operands match but no result does, the error is `No matching overload for OPERATOR +(T, U) returning R`. A cast `(R) expr` only prefers R and otherwise selects by operands before converting. Without an expected result type, such as OUTPUT or an operand of another operator, a unique operand match is selected; definitions that differ only by return type need a `RETURNS DEFAULT type` declaration to avoid ambiguity. DEFAULT follows the same rules as ordinary functions and does not resolve global/member operator ties; see [cpsc-keyword-default(7)](cpsc-keyword-default.7.md).

The expression has the selected function's declared return type. Comparisons and logic may return any supported function result type: a comparison returning STRING can be printed or assigned to STRING, but a BOOLEAN context requires an overload returning BOOLEAN. CASE comparisons follow the same result-type rule and must return BOOLEAN. A VOID result may be discarded through an explicit call but cannot be used as a value.

Operators keep their existing precedence, operand count and associativity. Operands are evaluated once, from left to right; overloaded AND and OR evaluate both operands. Operators are independent: defining = does not define <>, and operands are never swapped to find a match. A matching overload takes precedence over the built-in operation; otherwise the existing built-in rules apply.

Member operators on variables, fields, array elements and pointer dereferences modify the original receiver. Class-valued expressions can also receive operator calls, supporting chains such as `(A + B) + C`. Temporary receivers are destroyed at statement end, including exception exits; ordinary DECLARE objects remain function-scoped. Copies and return values retain the ordinary class value semantics.

Explicit calls use `OPERATOR+(A, B)` for global definitions and `A.OPERATOR+(B)` for members. They also work as statements, optionally prefixed with CALL, to discard results. Class-valued call results support explicit member operator calls, such as `Make().OPERATOR+(B)`. Indexing, function-call, address-of and dereference operators cannot be overloaded.

A class may define `OVERLOAD PROCEDURE OPERATOR<-(BYREF Other : ClassName)` or an overload taking another exact static type. Assignment to a class lvalue selects this member by operand type — not by an expected result, since the operator is a procedure — before falling back to same-class assignment; STRING-bearing aggregates copy their owned string fields, while other fields retain their existing copy behavior. The procedure takes exactly one explicit parameter; BYREF may bind a designator or materialized temporary. Keyword NEW assignment retains its allocation behavior. Self-assignment handling belongs to the operator body.

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
