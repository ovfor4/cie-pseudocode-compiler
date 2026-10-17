# Multi-file regression

Human review: pending. Expected output was independently traced by AI.

These are compiler extensions, not Cambridge guide requirements. Raise destroys guard 2, then unwinding Outer destroys guard 1 before the entry catch reads Destroyed=2. Execution continues. CatchHere catches another Raise inside the middle file; only guard 2 is destroyed, yielding Destroyed=3, and the entry outer catch must not run. Every throw crosses a unit boundary and cleanup runs before catch output.
