# Stack lifetimes

Human review: pending. Expected output was independently traced by AI.

Loop re-declarations destroy 31 and 32 before reinitialization. Copy destroys local 20 then its shallow parameter copy 21. Early RETURN destroys record field 50, array elements 42 and 41, loop local 33, original rebound storage 2 and first local 1. Heap object 99 stays alive. Never is not constructed. Main destroys 100 after its final output.
