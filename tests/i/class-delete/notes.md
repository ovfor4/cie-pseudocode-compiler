# Keyword DELETE

Human review: pending. Expected output was independently traced by AI.

Explicit calls print only the outer method. Keyword DELETE prints outer, right, left. Invalid stack and stale heap addresses throw before any method runs. The array element uses an ordinary explicit method call.

Normal main exit destroys Items[2], Items[1], Stack and the original Obj storage; each prints pair, leaf:2, leaf:1.
