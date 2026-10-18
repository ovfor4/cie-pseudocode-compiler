# operator-members

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: operator overloading is a project extension; arguments match exact types.

Trace: chained additions produce 3 + 2 + 4 = 9. Unary minus returns -3; NOT deliberately returns negated. A method can use its private concatenation operator, yielding 3!. Box addition modifies the original receiver from 3 to 8. Explicit member addition then global subtraction yields 8; explicit global subtraction yields 6. Discarded unary/global calls produce no output. MakeBox prints make exactly once, then Number prints number exactly once; their result has value 4 + 2 = 6. A throwing global multiplication reaches CATCH.
