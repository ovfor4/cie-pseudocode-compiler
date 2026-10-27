# Conventions: text, tokens and lexing

Part of [Load-bearing conventions](../CONVENTIONS.md).

**Project text must be in English.** This applies to documentation, source comments,
user-facing messages, examples and test fixtures. Unicode test data must not contain
Chinese text. Keep user instructions in README files concise; put architecture,
implementation notes and AI guidance in the relevant `AGENTS/` directory.

**Tokens are raw `int`.** `Lexer::gettok()` returns negative values for the `Token`
enum (`include/cps/Lexer.h`) and non-negative values for literal ASCII chars (`'('`,
`'+'`, `'<'`). Parser and CodeGen switch on both forms interchangeably;
`BinaryExprAST` stores this same int as its operator. Payloads (identifier text,
literal values) arrive out-of-band via public `Lexer` fields (`IdentifierStr`,
`StringVal`, `NumVal`, `RealVal`, `CharVal`) that must be read before the next
`gettok()`.

**Keywords are UPPERCASE and case-sensitive.** `declare` silently lexes as an
identifier. Assignment is `<-` (ASCII only), equality is single `=`, inequality `<>`,
concatenation `&` (reaches the parser as raw ASCII 38 — no token). Identifiers use
ASCII case-insensitive lookup/comparison through `IdentifierMap`, `IdentifierSet`
and `sameIdentifier`; retain source spelling in ASTs and diagnostics. This includes
functions, types, enum values, fields and builtins, but not C_TYPE import strings
or preprocessor switch names.
Integer tokens carry a checked unsigned magnitude through lookahead; the parser
applies the sign and i64 range check, including the minimum value. String/char
literals support C++17 byte escapes and Unicode escapes (UTF-8 in STRING, ASCII only
in CHAR). Comments are `//` to end of line.

Identifiers may begin with an underscore. The case-insensitive `__cps_` prefix is
reserved in every user identifier, including EXTERN declarations.

**The lexer reads a caller-owned stream.** Construct it with `Lexer(std::istream &)`;
the stream must outlive the lexer. Lookahead, line number and error state belong to
each instance. The driver opens the source file; stdin is not used for compilation.

**Preprocessing precedes lexing.** `preprocess(std::istream &)` returns masked source,
per-line __LIB states and an error flag. PP_DEFINE/PP_UNDEF change case-sensitive,
file-local switches; PP_IFDEF/PP_IFNDEF/PP_ENDIF maintain a nested conditional stack.
Directives occupy a line, with optional leading whitespace and trailing // comments.
Disabled ordinary source is never tokenized; DEFINE/UNDEF in it have no effect.
Masking preserves byte lengths, CRLF and original source line numbers. The driver
owns the processed stream passed to Lexer. PP_* keywords remain reserved outside
valid directives. There is no macro expansion, value, PP_ELSE or switch propagation
through INCLUDE; line-leading # directives are errors.
