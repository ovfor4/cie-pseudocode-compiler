% CPSC-LANGUAGE-OVERVIEW(7)

# NAME

cpsc-language-overview - supported cpsc pseudocode syntax

# DESCRIPTION

Keywords are uppercase and case-sensitive. Identifiers, including built-in function names, are case insensitive. Indentation is for readability; blocks use explicit closing keywords. Identifiers begin with a letter and continue with letters, digits, or underscores.

Assign with ASCII `<-`: `Count <- Count + 1`. Compare with `=`, `<>`, `<`, `>`, `<=`, and `>=`. STRING comparisons compare contents, case-sensitively. Use `+`, `-`, `*`, and `/` for arithmetic; `/` always returns REAL. Use `&` to join strings.

Precedence, from highest to lowest: unary operators, `* / DIV MOD`, `+ - &`, comparisons, AND, OR. Parentheses override precedence.

Parenthesized type casts, such as (C_INT)Number, have unary precedence. C_INT, C_CHAR, C_STRING and C_DOUBLE are native C aliases; TYPE name = C_TYPE("...") imports other supported C base types and pointers. Conversions between C and pseudocode types must be explicit. These are compiler extensions; see [cpsc-keyword-c_type(7)](keyword/cpsc-keyword-c_type.7.md).

Literals include integers, decimal real numbers, TRUE/FALSE, strings in double quotes, and single-byte characters in single quotes. Negative numbers use unary minus. There are no string or character escape sequences. Comments begin with `//` and continue to the end of the line.

Declare variables before use. See DECLARE for built-in types and arrays, TYPE for records/enumerations/pointers, and CLASS for public/private fields.

The compiler extensions TRY/CATCH/ENDTRY and THROW support anonymous exceptions with stack unwinding. Runtime check failures print the source line and error reason to standard output before throwing and can be caught. An uncaught exception also prints a generic termination message and returns status 1. There is no error object or what() support.

# KEYWORDS

- [cpsc-keyword-constant(7)](keyword/cpsc-keyword-constant.7.md): declare a named literal with block scope.
- [cpsc-keyword-declare(7)](keyword/cpsc-keyword-declare.7.md): declare variables and arrays.
- [cpsc-keyword-input(7)](keyword/cpsc-keyword-input.7.md): read a value from standard input.
- [cpsc-keyword-output(7)](keyword/cpsc-keyword-output.7.md): print values to standard output.
- [cpsc-keyword-case(7)](keyword/cpsc-keyword-case.7.md): match values or ranges in order.
- [cpsc-keyword-if(7)](keyword/cpsc-keyword-if.7.md): choose between branches.
- [cpsc-keyword-try(7)](keyword/cpsc-keyword-try.7.md): catch or throw exceptions.
- [cpsc-keyword-while(7)](keyword/cpsc-keyword-while.7.md): repeat while a condition is true.
- [cpsc-keyword-repeat(7)](keyword/cpsc-keyword-repeat.7.md): repeat until a condition is true.
- [cpsc-keyword-for(7)](keyword/cpsc-keyword-for.7.md): iterate over an integer range.
- [cpsc-keyword-function(7)](keyword/cpsc-keyword-function.7.md): define a function that returns a value.
- [cpsc-keyword-procedure(7)](keyword/cpsc-keyword-procedure.7.md): define a procedure and parameter modes.
- [cpsc-keyword-call(7)](keyword/cpsc-keyword-call.7.md): invoke a procedure.
- [cpsc-keyword-openfile(7)](keyword/cpsc-keyword-openfile.7.md): open a text file.
- [cpsc-keyword-readfile(7)](keyword/cpsc-keyword-readfile.7.md): read a line from a text file.
- [cpsc-keyword-writefile(7)](keyword/cpsc-keyword-writefile.7.md): write a line to a text file.
- [cpsc-keyword-closefile(7)](keyword/cpsc-keyword-closefile.7.md): close a text file.
- [cpsc-keyword-type(7)](keyword/cpsc-keyword-type.7.md): define enumerations, pointers, and records.
- [cpsc-keyword-c_type(7)](keyword/cpsc-keyword-c_type.7.md): import native C types and explicitly cast values.
- [cpsc-keyword-class(7)](keyword/cpsc-keyword-class.7.md): define a class with public and private fields.
- [cpsc-keyword-and(7)](keyword/cpsc-keyword-and.7.md): combine conditions with logical AND.
- [cpsc-keyword-or(7)](keyword/cpsc-keyword-or.7.md): combine conditions with logical OR.
- [cpsc-keyword-not(7)](keyword/cpsc-keyword-not.7.md): negate a condition.
- [cpsc-keyword-div(7)](keyword/cpsc-keyword-div.7.md): perform integer division.
- [cpsc-keyword-mod(7)](keyword/cpsc-keyword-mod.7.md): find the integer division remainder.

Closing and auxiliary keywords are documented with their main constructs: THEN/ELSE/ENDIF with IF; OF/OTHERWISE/ENDCASE with CASE; CATCH/ENDTRY/THROW with TRY; ENDWHILE with WHILE; UNTIL with REPEAT; TO/STEP/NEXT with FOR; RETURN/RETURNS/BYVAL/BYREF and function/procedure closing keywords with FUNCTION/PROCEDURE; READ/WRITE/APPEND with OPENFILE; ARRAY/OF and INTEGER/REAL/BOOLEAN/CHAR/STRING with DECLARE; ENDTYPE with TYPE; PUBLIC/PRIVATE/ENDCLASS with CLASS. TRUE/FALSE are BOOLEAN literals.

# BUILT-IN FUNCTIONS

- [cpsc-function-length(7)](function/cpsc-function-length.7.md): find a string's length.
- [cpsc-function-mid(7)](function/cpsc-function-mid.7.md): extract part of a string.
- [cpsc-function-left(7)](function/cpsc-function-left.7.md): extract the beginning of a string.
- [cpsc-function-right(7)](function/cpsc-function-right.7.md): extract the end of a string.
- [cpsc-function-lcase(7)](function/cpsc-function-lcase.7.md): convert text to lower case.
- [cpsc-function-ucase(7)](function/cpsc-function-ucase.7.md): convert text to upper case.
- [cpsc-function-asc(7)](function/cpsc-function-asc.7.md): find a character's byte code.
- [cpsc-function-chr(7)](function/cpsc-function-chr.7.md): convert a byte code to text.
- [cpsc-function-is_num(7)](function/cpsc-function-is_num.7.md): test whether text represents a number.
- [cpsc-function-num_to_str(7)](function/cpsc-function-num_to_str.7.md): convert a number to text.
- [cpsc-function-str_to_num(7)](function/cpsc-function-str_to_num.7.md): convert text to a real number.
- [cpsc-function-eof(7)](function/cpsc-function-eof.7.md): test for the end of a text file.
- [cpsc-function-malloc(7)](function/cpsc-function-malloc.7.md): allocate raw memory.
- [cpsc-function-free(7)](function/cpsc-function-free.7.md): release an explicit allocation.

# UNSUPPORTED FEATURES

DATE, SET types and DEFINE, random-file operations (RANDOM/SEEK/GETRECORD/PUTRECORD), INT, RAND, string escapes, whole-array assignment or return, array fields, and class methods/constructors/destructors/inheritance (NEW/INHERITS/SUPER).

The individual pages note relevant differences from the Cambridge 2026 pseudocode guide. This manual describes the compiler's current support.

# EXAMPLE

```text
DECLARE Count : INTEGER
Count <- 2
IF Count > 0 THEN
    OUTPUT "Count: " & NUM_TO_STR(Count)
ENDIF
```

# SEE ALSO

[cpsc(1)](../cpsc.1.md).
