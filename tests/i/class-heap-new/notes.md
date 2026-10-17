# Heap NEW and object storage

Human review: pending. Expected output was independently traced by AI.

Project semantics extend guide section 10.2. Each Box constructor sets Value to 1. Rebinding A preserves the old stack object's 7 while the new object becomes 9. Ordinary A <- B copies 4 into the current heap object, leaving B and the old stack object independent. BYVAL copies 4, then independently rebinds and changes its local object to 20. BYREF NEW copies 1 into A's current object, observed through CurrentA. Replacing this likewise copies into that same object; explicit Reset calls the ordinary constructor method. Array-element, field, bare field alias and pointer-dereference NEW assignments preserve their destination addresses and copy 1. Named's argument is 42. Failing throws in both DECLARE and heap construction, reaching the surrounding catches. Loop DECLARE restores its original stack object and constructs it on every iteration, so the previous heap object's 99 is never printed.

Fragile's failed constructor leaves Stable bound to its original object, whose Value remains 71 through both names. Heap construction of Holder constructs its Box field as 1 while the old Holder's field remains 80 through Field. Ordered prints first, second, outer for stack and heap construction, proving field declaration order before the enclosing constructor. PrivateFactory's unused Build method checks that same-class access permits private automatic and explicit construction.

A method-only Marker class has no fields, but heap NEW must still provide a non-null object address so a pointer receiver can call Get and print 99.
