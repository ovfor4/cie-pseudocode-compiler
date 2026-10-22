# extern-c-struct

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: compiler extension (EXTERN "C", INCLUDE_OBJ of a user object); a record whose fields are all C types has the layout of the matching C struct.

Trace: the harness compiles `lib.c` with the host C compiler. Sample matches `struct Sample {char; double; int}`, including the padding after the char. S is 'S', 1.5, 5. scale_sample(^S, 3) multiplies the value through the pointer to 4.5 and leaves tag and count alone, so the first line is `S 4.500000 5`. shared_sample returns the address of the static struct in lib.c, initialised to 'G', 0.5, 7, giving `G 0.500000 7`. P^.Count <- 9 writes through that pointer into the C object, so shared_count() returns 9.
