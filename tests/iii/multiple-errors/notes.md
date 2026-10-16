# multiple-errors

Lexer and parser must both diagnose independent errors and recover to subsequent statements without hanging or producing output.

Require exit 1, the stated cause and source line on the same diagnostic, exactly one compilation termination line, and preservation of absent/existing outputs for both native and IR requests. Missing block delimiters are located at EOF (the line after the final newline).
