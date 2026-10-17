# Cross-file runtime-check regression

Human review: pending. Expected output was independently traced by AI.

TRY/CATCH and multi-file builds are compiler extensions. Quotient(0) fails at the division on library line 2, printing the exact runtime diagnostic before throwing. The entry catches it and continues. Quotient(2) then returns 5, proving the handler leaves execution usable.
