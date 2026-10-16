# string-extensions

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: Project builtin manuals; LEFT, ASC, CHR, IS_NUM and numeric string conversions are extensions, not specified by this CIE guide.

Trace: LEFT keeps two or zero bytes; ASCII A=65 and Z=90. Only 12.5 is a fully consumed nonempty number. Integer conversion preserves all 19 digits; REAL is observed only through equality.
