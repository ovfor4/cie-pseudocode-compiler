# unwind-calls

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: Project exception extension and C++ unwinding ABI.

Trace: Fail unwinds into the caller catch, which returns 7 while finishing that catch. A new throw inside the nested catch transfers to the outer catch.
