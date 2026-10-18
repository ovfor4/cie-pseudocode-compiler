# Trace notes

Review status: pending human review.

Simple escapes produce a literal quote, backslash and question mark. Octal 101 and hexadecimal 42 add A and B after the leading A; Unicode escapes encode the displayed accented letter and emoji. Escaped CHAR byte values are 10, 9 and 255. Built-in STRING stops at the embedded NUL.
