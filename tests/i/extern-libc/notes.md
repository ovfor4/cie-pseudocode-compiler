# extern-libc

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: compiler extension (EXTERN "C", INCLUDE_OBJ "__libc"); C library semantics.

Trace: srand(1) has no output. puts prints "hello" and a newline. abs(-5)=5. sqrt(16)=4.0 printed as REAL-style C double "%f". printf receives int 42, "x", float 1.5 promoted to double, short -7 promoted to int, prints "42 x 1.5 -7|" (12 characters) with no newline; OUTPUT "" ends that line, then 12 is printed.
