CLASS Item
    Value : INTEGER
ENDCLASS
TYPE Raw = C_TYPE("void*")
DECLARE Obj : Item
MALLOC(8)
REALLOC((Raw)0, 8)
FREE((Raw)0)
Obj <- NEW Item()
DELETE Obj
