# Destructor regression

Human review: pending. Expected output was independently traced by AI.

Nested frames unwind in reverse local order before main catches. The earlier loop call sees a later DECLARE alive on iteration two. A same-function catch keeps locals alive. The uncaught path cleans procedure frames and deliberately omits main object 100.
