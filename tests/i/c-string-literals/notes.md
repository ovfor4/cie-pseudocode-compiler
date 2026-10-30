# C string literals

Human review: pending. Expectations follow the literal bytes and declared overloads.

The adjacent uppercase C suffix produces C_STRING. Escapes use the ordinary lexer rules. Explicit STRING casts preserve the text; an empty C string has length zero after conversion. Both ordinary and suffixed literals have type C_STRING, so both calls select the exact C_STRING overload.
