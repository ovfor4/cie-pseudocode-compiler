INCLUDE "bridge.cps"
INCLUDE "bridge.cps"
DECLARE Item : Container
Item.Value <- 8
OUTPUT Item.Read(), ":", Twice(Base), ":", First
DECLARE Node : Pair
Node.Value <- Item.Read()
OUTPUT Node.Value
