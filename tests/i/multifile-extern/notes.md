# multifile-extern

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: compiler extension; identical EXTERN declarations may repeat across files, as C declarations may.

Trace: abs(-3)=3. Twice(-4) returns abs(-4)+abs(-4)=8.
