# calls

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: CIE pp. 22-24; array parameter syntax follows the project manual.

Trace: Sticky BYREF changes A and B, while C is copied. 5!=120. BYVAL array mutation is isolated; BYREF writes 4+3 into the second caller element.
