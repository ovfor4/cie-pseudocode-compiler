# arithmetic

CIE pp. 15-16, project i64 and truncation-towards-zero DIV/MOD. Full-width extremes are used only in safe division/remainder and comparisons; addition, subtraction and multiplication use A MOD 1000001. B is nonzero, and INT64_MIN / -1 is never generated.

Each record produces exactly one output line. Random records are preceded by fixed boundary records; every arithmetic intermediate is bounded.
