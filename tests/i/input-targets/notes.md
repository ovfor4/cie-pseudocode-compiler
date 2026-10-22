# INPUT targets

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: CIE guide p. 15 section 5.1 allows individual data-structure elements. Native C input, token-based scalar input, rest-of-line STRING input and exceptions follow project behavior.

Trace: the loop stores 10, 20, 30; the grid receives 40. Student 2 receives 50, the line Ada, true, A, 2.5 and -7, then its score becomes 80 through P. The class fields receive 60 and 70. NextIndex prints once and selects Scores[1], which receives 90. Bounds and null-pointer failures occur before reading input, leaving 100 for the final valid input.
