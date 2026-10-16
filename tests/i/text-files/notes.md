# text-files

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: CIE p. 25; CRLF input handling follows project text-file semantics.

Trace: Input contains three lines, including an empty line and a last line without terminator. Empty-file EOF is true. WRITE replaces old contents; APPEND adds last after the existing blank line.

WHILE headers use the standard CIE spelling without DO. The separate standard-while fixture also checks this syntax.
