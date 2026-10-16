# integer-boundaries

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: Project INTEGER=i64 contract; CIE pp. 7 and 15 do not specify bit width or overflow.

Trace: Inputs are 2^63-1 and -2^63. Every operation stays in range; their sum is -1 and division by 1 preserves both values.
