# String values

Human review: pending. Expected output is traced from independent string values.

Record copies and BYVAL array calls must preserve the source when a copy changes. Self-assignment preserves the value. Global and constant strings remain available to functions. Comparisons use case-sensitive content ordering. Ordinary literals retain the legacy NUL truncation; CHR(0) returns a NUL CHAR that converts to a one-byte NUL string.
