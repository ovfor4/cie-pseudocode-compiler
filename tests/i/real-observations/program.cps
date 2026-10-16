FUNCTION Scale(Value : REAL) RETURNS REAL
    RETURN Value * 2.0
ENDFUNCTION
DECLARE R : REAL
R <- 5 / 2
OUTPUT R = 2.5, ":", R > 2.0, ":", Scale(R) = 5.0
OUTPUT (0.5 + 0.25) = 0.75, ":", (-0.0 = 0.0)
