# overload-functions

OVERLOAD is a project extension. The scalar and array overloads of Score are defined in separate imported files. Each input record exercises INTEGER, REAL and STRING argument types; different argument counts; FUNCTION and PROCEDURE sharing a name; BYREF scalar mutation; array element types and ranks; case-insensitive calls; and nested calls whose static result type selects the outer overload.

Inputs are A, B, Size, Lower and a nonempty ASCII string. Integer array elements form the progression A + k*B for k from 0 to Size-1. REAL elements are those exact integers divided by eight. The independent C++ oracle uses the closed-form progression sum Size*A + B*Size*(Size-1)/2, its sign, and the simplified weighted matrix sum 8*A+B. The nested scalar result is 9*A+4, and the BYREF procedure changes only its destination to A+B*Size.

Each record emits one colon-separated line. A and B stay within [-1000,1000], Size within [1,8], and Lower within [-8,8]; all arithmetic is safely bounded. Binary fractions are exactly representable, and REAL results are observed through BOOLEAN output. Fixed records cover zero, range endpoints, changing array bounds, positive/negative totals and exact cancellation. The SHA-256 generator adds the requested random count.
