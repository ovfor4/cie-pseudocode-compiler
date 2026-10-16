# arithmetic

CIE pp. 15-16, project i64 and truncation-towards-zero DIV/MOD. Full-width extremes are used only in safe division/remainder and comparisons; addition, subtraction and multiplication use A MOD 1000001. B is nonzero, and INT64_MIN / -1 is never generated.

Each record produces exactly one output line. Random records are preceded by fixed boundary records; every arithmetic intermediate is bounded.

Nested IF classifies the sign; CASE tests an inclusive -10..10 range before an overlapping zero label, so zero must select the first range. The oracle computes these results without reproducing the pseudocode control flow.
