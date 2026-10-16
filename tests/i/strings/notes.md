# strings

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: CIE pp. 7, 16-17; byte ordering of ASCII strings is a project convention.

Trace: The empty string has length 0; MID positions 2..4 are BCD; the last three characters are FGH. Case conversion changes only the letters. Concatenation creates equal contents.
