# Trace notes

Review status: pending human review.

Assignments copy 3 into B, self-assign 3, then copy the returned 7. C_STRING assignment sets A to length 5 without a copy trace. Record-field, array-element and dereferenced assignments trace 7, 5 and 7. Final fields are 7, 7, 7 and 5. This is the member assignment extension.
