TYPE Season = (Spring, Summer)
DECLARE S : Season
S <- Summer
TRY
    S <- S + 1
CATCH
    OUTPUT "enum caught"
ENDTRY
OUTPUT S = Summer
