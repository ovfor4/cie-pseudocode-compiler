# extern-c-object

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: compiler extension (EXTERN "C", INCLUDE_OBJ of a user object); C semantics.

Trace: the harness compiles `lib.c` with the host C compiler (`objects` in `case.json`). add3(1,2,3)=6. scale(2.5,4)=10.0, printed like other C doubles as `10.000000`. name_length("cambridge")=9. next_call increments a static counter in the object, giving 1 then 2. say calls puts, which prints the string and a newline.
