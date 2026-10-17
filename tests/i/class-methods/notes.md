# class-methods

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: guide section 10.1; this and existing class value semantics are project behavior.

Trace: A and B start at 3 and 8; Add changes only A to 5. Copy then Set leaves A at 9 and B at 8. Clone copies 9 into B; same-class private access reads 9. Sum(4) is 10 and the global Get returns 100. The array pointer changes Items[1] from 6 to 10; the nested object stays 7. Take adds A's 9 to Number's 1 and writes 10 into Values[1]. Fail is caught. Passing this BYREF changes A to 42. A method-only class returns 99 from its independent Get method.
