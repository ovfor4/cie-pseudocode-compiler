% CPSC-KEYWORD-EXTERN(7)

# NAME

cpsc-keyword-extern, cpsc-keyword-extern_function, cpsc-keyword-extern_procedure - declare a C function

# SYNOPSIS

```text
EXTERN "C" EXTERN_FUNCTION name(type, ...) RETURNS type
EXTERN "C" EXTERN_PROCEDURE name(type, ...)
```

# DESCRIPTION

EXTERN is a compiler extension, allowed only at top level. It declares a C function without a body; there is no ENDFUNCTION or ENDPROCEDURE. The name is the exact C symbol; calls from pseudocode remain case insensitive. Call an EXTERN_FUNCTION in an expression and an EXTERN_PROCEDURE with CALL.

Parameters list only types. Parameter and result types must be C types (C_INT, C_CHAR, C_STRING, C_DOUBLE or a C_TYPE alias, including pointers), C struct records, or pointer types such as `TYPE PInt = ^C_INT` whose chain finally points to either. Pointers to pseudocode types such as `^INTEGER` and pointer cycles such as `TYPE P = ^P` are rejected. void is not allowed; use EXTERN_PROCEDURE for a C function returning void. EXTERN_FUNCTION requires RETURNS and EXTERN_PROCEDURE does not accept it. BYREF and BYVAL are unsupported; to let C modify a variable, declare a pointer parameter and pass ^Variable instead. Pseudocode values must be cast, as for other C values.

A C struct record is a TYPE record, not a CLASS, whose fields all have such types; a pointer field may refer to its own record, as in a linked list. A record with any other field, such as INTEGER, REAL, CHAR, BOOLEAN, STRING, an enumeration or a CLASS, is rejected with an error naming the field, for example `EXTERN parameter 1 of puts uses record Person, which is not a C struct: field Name of Person has type STRING`. cpsc does not read C headers: declare the fields in the same order and with the same C types as the C struct. A C struct record passes by value, as a copy, or through a pointer type to the record, which lets the C function change the caller's variable; neither needs a cast. Both work for parameters, RETURNS and variadic arguments. The calling convention is that of the platform's C compiler, computed with Clang's C ABI rules.

A final `...` makes the function variadic. Variadic arguments must also be C values, C struct records or pointers to them; `^x` passes a plain pointer when x is a C value or a C struct, like C's `&x`. C scalars receive C default argument promotions (float to double, narrow integers and _Bool to int or unsigned int). As in C, records and pointers are not promoted; the C function reads them with `va_arg(ap, struct S)` or `va_arg(ap, struct S *)`.

Every EXTERN symbol must be defined by an object named with INCLUDE_OBJ, otherwise compilation fails. EXTERN cannot be combined with OVERLOAD or share a name with a FUNCTION or PROCEDURE. As in C, the same function may be declared again in the same or another input file when the signature is identical (equivalent C aliases match); conflicting declarations are errors. Each file sees its own declarations and those of the files it includes.

# EXAMPLE

```text
INCLUDE_OBJ "__libc"
EXTERN "C" EXTERN_FUNCTION printf(C_STRING, ...) RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION abs(C_INT) RETURNS C_INT
DECLARE Count : C_INT
Count <- printf((C_STRING)"%d|", abs((C_INT)-5))
OUTPUT ""
```

`vec.c`, compiled with `cc -c vec.c`:

```text
struct Vec2 { double x, y; };
double vec2_dot(struct Vec2 a, struct Vec2 b) { return a.x * b.x + a.y * b.y; }
void vec2_scale(struct Vec2 *v, double k) { v->x *= k; v->y *= k; }
```

`me.cps`:

```text
INCLUDE_OBJ "vec.o"
TYPE Vec2
    DECLARE X : C_DOUBLE
    DECLARE Y : C_DOUBLE
ENDTYPE
TYPE Vec2Ptr = ^Vec2
EXTERN "C" EXTERN_FUNCTION vec2_dot(Vec2, Vec2) RETURNS C_DOUBLE
EXTERN "C" EXTERN_PROCEDURE vec2_scale(Vec2Ptr, C_DOUBLE)
DECLARE V : Vec2
V.X <- (C_DOUBLE)1.5
V.Y <- (C_DOUBLE)2
CALL vec2_scale(^V, (C_DOUBLE)2)
OUTPUT vec2_dot(V, V)
```

Run with `cpsc me.cps -r` to print `25.000000`.

# SEE ALSO

[cpsc-keyword-include_obj(7)](cpsc-keyword-include_obj.7.md), [cpsc-keyword-c_type(7)](cpsc-keyword-c_type.7.md), [cpsc-keyword-type(7)](cpsc-keyword-type.7.md), [cpsc-keyword-call(7)](cpsc-keyword-call.7.md), [cpsc-language-overview(7)](../cpsc-language-overview.7.md).
