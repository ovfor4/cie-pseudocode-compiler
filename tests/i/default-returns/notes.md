# Default returns

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: DEFAULT and return-type overloading are project extensions.

Trace: Foo(INTEGER) defaults to REAL 2.5, while Foo(REAL) defaults to INTEGER 4. INTEGER assignment and cast select 3; a C_INT cast falls back to the default 2.5 then converts to 2. Get defaults to 6.5, member addition to 8.5, and global negation to 10.5; INTEGER assignments select 7, 9 and 11. The nested Foo call defaults to REAL, selecting Consume(REAL), which returns 12.
