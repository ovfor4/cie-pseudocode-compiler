DECLARE Zero : REAL
Zero <- -0.0
TRY
    OUTPUT (1.0 / Zero) = 0.0
CATCH
    OUTPUT "zero caught"
ENDTRY
