# uncaught-error

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: Project runtime-check and final-handler contract.

Trace: MOD by zero throws on line 3. The final handler prints termination and returns 1; unreachable is never printed.
