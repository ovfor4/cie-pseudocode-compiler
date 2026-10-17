# overload-functions

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: OVERLOAD is a project extension.

Trace: INTEGER adds one; positive REAL returns TRUE; two INTEGERs return pair; STRING prints text. Rank-one INTEGER, rank-two INTEGER and REAL arrays select distinct overloads and yield 7, 9 and TRUE. Nested Pick(1) returns INTEGER 2, so the outer call yields 3.

Bump selects INTEGER and mutates 4 to 5 through BYREF. The ordinary Legacy function still converts INTEGER 1 to REAL and returns TRUE.
