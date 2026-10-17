INCLUDE "lib.cps"
OUTPUT Add(1, 2)
OUTPUT Counter, ":", Label, ":", Enabled, ":", Initial, ":", Ratio = -1.5
CALL Increment()
OUTPUT Counter
CALL Shadow()
OUTPUT Counter
DECLARE Counter : INTEGER
Counter <- 99
CALL Increment()
OUTPUT Counter, ":", ReadCounter()
