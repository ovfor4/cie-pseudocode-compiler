# constant-scope

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: CIE p. 8 allows restricting a constant scope; project lexical binding and function inheritance rules.

Trace: The inner constant shadows 3 only within the IF. The outer binding and the function both retain 3.
