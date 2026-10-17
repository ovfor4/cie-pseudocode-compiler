# overload-methods

OVERLOAD is a project extension. Each record exercises NEW with zero arguments, INTEGER, STRING and two INTEGER arguments; overloaded getters with different result types; overloaded setters; nested this.Get/this.Set calls; explicit constructor calls; keyword NEW and DELETE; and parameterless construction of record fields and array elements.

Inputs are A, B, Size and a nonempty ASCII string. The independent C++ oracle models resulting values directly: default construction yields 5, INTEGER construction yields 2*A+3, STRING construction yields length+20, and two-argument construction yields A-B. Default array values sum to 5*Size. Get(INTEGER) adds 3*B; Get(REAL) compares the stored value with B/8; Get(STRING) appends an exclamation mark. Set(STRING) stores length. Set(A) followed by Apply(B) yields A+3*B. Explicit NEW(STRING) rebuilds the receiver's value as length+20.

Each record emits one colon-separated line. A and B stay within [-1000,1000], Size within [1,8], and string length within [1,24] for random records. All arithmetic is safely bounded; REAL comparisons use exactly representable binary fractions. Fixed records include equal, immediately smaller and immediately larger comparison thresholds, zero and range endpoints. The SHA-256 generator adds the requested random count.
