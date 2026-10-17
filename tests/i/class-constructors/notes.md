# Class constructors

Human review: pending. Expected output was independently traced by AI.

The project extends guide section 10.2 with automatic stack, field and array construction. Leaf observes its zeroed Value before setting it to 3. A and B each construct once; Named stores name; Plain has no constructor and stays zero. Four array elements each construct once. Wrapped constructs Pair's two Leaf fields in order before Pair observes their sum of 6. Each execution of the loop declaration zeroes and constructs Local again, so both iterations print leaf:0 then 3 despite storing 99 in the previous iteration.
