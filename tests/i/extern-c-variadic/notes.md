# extern-c-variadic

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: compiler extension (EXTERN "C", INCLUDE_OBJ of a user object); C11 6.5.2.2p6-7 default argument promotions and 7.16.1.1 va_arg.

Trace: the harness compiles `lib.c` with the host C compiler. sum_ints reads 5 arguments with va_arg(ap, int), so each must arrive promoted to int: unsigned char 200 gives 200, short -7 gives -7, unsigned short 65535 gives 65535 (int is wider than short), _Bool TRUE gives 1 and char 'A' gives 65. 200 - 7 + 65535 + 1 + 65 = 65794. sum_doubles reads 2 arguments with va_arg(ap, double): float 1.5 is promoted to double 1.5, and double 2.25 is passed unchanged. 1.5 + 2.25 = 3.75, printed like other C doubles as `3.750000`.
