TYPE Raw = C_TYPE("void*")
DECLARE P : Raw
P <- MALLOC(8)
FREE(P)
FREE(P)
