# operator-kinds

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: operator overloading is a project extension.

Trace: each binary operator on Choice explicitly returns its own marker 101 through 115 in source order. Unary minus returns 116 and NOT returns 117. The explicit AND call returns 114. In particular /, &, comparisons and logic must use the declared INTEGER result, overriding builtin type inference and enum comparisons.
