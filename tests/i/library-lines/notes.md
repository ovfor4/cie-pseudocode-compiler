# Trace notes

Review status: pending human review.

User calls on lines 3, 8 and 13 enter nested library array access, an array-argument contract and a close of an unopened file. The declaration on line 18 automatically invokes a library constructor that divides by zero. Each error uses that user line and is caught. These are library and exception extensions.
