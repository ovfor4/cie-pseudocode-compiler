CLASS Item
    Id : INTEGER
    PROCEDURE DELETE()
        OUTPUT "item:", Id
    ENDPROCEDURE
    PROCEDURE Explicit()
        this.DELETE()
    ENDPROCEDURE
    PROCEDURE Release()
        DELETE this
    ENDPROCEDURE
ENDCLASS
TYPE ItemPtr = ^Item
DECLARE Obj : Item
DECLARE Ptr : ItemPtr
Obj <- NEW Item()
Obj.Id <- 1
Ptr <- ^Obj
CALL Obj.Explicit()
DELETE Ptr^
Obj <- NEW Item()
Obj.Id <- 2
CALL Obj.Release()
DECLARE Arr : ARRAY[1:2] OF Item
Arr[2].Id <- 3
TRY
    DELETE Arr[2]
CATCH
    OUTPUT "array caught"
ENDTRY
