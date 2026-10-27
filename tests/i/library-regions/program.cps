INCLUDE "regions.cps"
CALL Mixed()
TRY
    CALL TemplateError<INTEGER>()
CATCH
    OUTPUT 5
ENDTRY
TRY
    CALL DeleteOnExit()
CATCH
    OUTPUT 6
ENDTRY
