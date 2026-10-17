% CPSC-KEYWORD-C_TYPE(7)

# NAME

cpsc-keyword-c_type - import a native C type as a pseudocode alias

# SYNOPSIS

```text
TYPE name = C_TYPE("C type expression")
(type) expression
```

# DESCRIPTION

C_TYPE and parenthesized casts are compiler extensions. C_TYPE requires a string literal and appears only on the right side of a top-level TYPE declaration. C_INT, C_CHAR, C_STRING and C_DOUBLE are predefined aliases for int, char, char* and double. Imported aliases for the same C type are interchangeable, including BYREF arguments and array parameters; the original pseudocode TYPE rules remain nominal.

Supported base types are _Bool; char, signed char and unsigned char; signed and unsigned short, int, long and long long; and float, double and long double. Legal C combinations such as unsigned, signed int and unsigned long int are accepted. Type spelling is case-sensitive. void is allowed for pointer pointees and function return types, but not variables, fields or parameters. Multiple pointer stars may precede or follow the base type: `"**char"` and `"char**"` are equivalent. Whitespace is allowed. Mixed star placement, const/volatile, structs, unions, C typedef names, array types, function pointers, complex types and compiler-specific types are unsupported.

C widths, char signedness and floating-point formats come from the compiler's build platform. Generated programs must use the same platform ABI; cross-target compilation is not implemented. EXTERN declarations use these types to call C functions; see [cpsc-keyword-extern(7)](cpsc-keyword-extern.7.md). INTEGER remains signed 64-bit, CHAR remains an unsigned byte, and REAL remains double precision. In particular, C_CHAR need not have CHAR's signedness.

A cast has unary precedence and converts its following operand. Declare a type alias before using its name in a cast. Numeric, character and boolean values can be explicitly converted between pseudocode and C types. STRING can be cast to char* and back, and C pointer types can be cast to other C pointer types. C void* and original pseudocode pointer types can be explicitly cast to each other. Pointer/integer casts and casts involving pseudocode enums or whole records are unsupported. Casts are values, not assignable locations.

C/pseudocode assignment, argument passing and mixed arithmetic require an explicit cast, including ordinary literals: use n <- (C_INT)1 and n + (C_INT)1. Different C scalar types use C++ numeric conversions, integer promotions and usual arithmetic conversions. BYREF requires the same type and performs no conversion. Comparisons and logical operations return BOOLEAN. The / operator always performs real division and returns REAL, including with two C integer operands; DIV and MOD accept C integers after promotion. C values used directly as IF/WHILE conditions need a cast to BOOLEAN.

C scalar values support INPUT and OUTPUT. Plain char prints and reads a character; signed char and unsigned char use numeric input/output. _Bool uses the existing TRUE/FALSE output and numeric input. C pointers start null and support prefix ^ for address-of, suffix ^ for dereference, and = or <> between the same C pointer type. Dereferencing null throws; void pointers cannot be dereferenced. Pointer arithmetic and direct pointer INPUT/OUTPUT are unsupported. To print a C string, cast it to STRING. STRING/char* casts share the original buffer without allocation; a null char* becomes an empty STRING. Pointer validity and writable storage remain the caller's responsibility.

# EXAMPLE

```text
TYPE NativeInt = C_TYPE("int")
TYPE NativeIntPtr = C_TYPE("*int")
DECLARE Number : NativeInt
DECLARE Pointer : NativeIntPtr
DECLARE Text : C_STRING
Number <- (C_INT)42
Pointer <- ^Number
Pointer^ <- Number + (C_INT)1
Text <- (C_STRING)"Result"
OUTPUT (STRING)Text, ": ", Number
OUTPUT Number / (C_INT)2
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-type(7)](cpsc-keyword-type.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md), [cpsc-keyword-input(7)](cpsc-keyword-input.7.md), [cpsc-keyword-output(7)](cpsc-keyword-output.7.md).
