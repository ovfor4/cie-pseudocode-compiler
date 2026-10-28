# Contextual conversions

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: the function table in reference/9618_w25_in_21.pdf permits REAL/INTEGER results from STR_TO_NUM and STRING/CHAR results from NUM_TO_STR. DEFAULT selection and truncation toward zero for INTEGER conversion are project rules.

Trace: context-free numeric parsing returns REAL 23.45 and 7.0; INTEGER assignments truncate 23.45 and -23.45 to 23 and -23, and parse character '7' as 7. Default text formatting produces 7 and 7.0. CHAR assignment accepts the one-character integer representation. STRING assignment retains 12. The two longer CHAR results, invalid numeric text and INTEGER overflow throw at their caller lines and enter their respective catches.
