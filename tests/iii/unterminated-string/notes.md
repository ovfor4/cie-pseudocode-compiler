# unterminated-string

Invalid string literal; CIE p. 7 and project lexer contract.

Require exit 1, the stated cause and source line on the same diagnostic, exactly one compilation termination line, and preservation of absent/existing outputs for both native and IR requests. Missing block delimiters are located at EOF (the line after the final newline).
