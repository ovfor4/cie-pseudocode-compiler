INCLUDE "operators.cps"
DECLARE item : Key
DECLARE low : Key
DECLARE high : Key
item.Value <- 3
low.Value <- 1
high.Value <- 4
OUTPUT item + 2
OUTPUT item = high
CASE OF item
low : OUTPUT "wrong"
low TO high : OUTPUT "range"
OTHERWISE : OUTPUT "wrong"
ENDCASE
DECLARE items : ARRAY[1:2] OF Key
items[2].Value <- 4
OUTPUT items[2] = high
TYPE KeyPointer = ^Key
DECLARE pointer : KeyPointer
pointer <- ^item
OUTPUT pointer^ + 5
