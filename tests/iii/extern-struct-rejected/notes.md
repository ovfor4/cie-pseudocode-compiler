# extern-struct-rejected

Compiler extension: an EXTERN signature accepts a record, through a pointer type or by value, only as a C struct (every field a C type, a C struct or a pointer type to one). Person has a STRING field, so both a pointer to it and Person by value are rejected and the field is named.

Require exit 1 and each cause on its EXTERN line.
