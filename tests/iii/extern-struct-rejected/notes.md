# extern-struct-rejected

Compiler extension: an EXTERN signature accepts a record only as a C struct (every field a C type, a C struct or a pointer type to one) and, for now, only through a pointer type. Person has a STRING field, so a pointer to it is rejected and the field is named. Point is a C struct, but passing it by value is not supported yet.

Require exit 1 and each cause on its EXTERN line.
