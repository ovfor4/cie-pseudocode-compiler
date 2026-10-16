# Compiler test findings

Baseline: Linux x86_64, Clang/LLVM 23.1.3, Release EMBEDDED, compiler sources at `e2c6570`. Tests do not modify compiler sources. The initial suite had 45 compiler cases plus harness and portable-generator checks. Fixed output expectations are pending human review.

The expanded suite has 67 compiler cases plus harness and portable-generator checks (69 CTest entries). The 13 failures below were reproduced with the Actions seed `e1e789ac0249149a4aa1671f5f9260d488cb617a` and are now fixed on Linux Release EMBEDDED. Regression cases cover case-insensitive name domains and collisions, default/public/private fields, checked i64 literals in expressions/constants/parameter bounds, rejection of WHILE DO, and nested/multiline diagnostic locations. Existing fixed expectations and differential generators remain unchanged; WHILE fixtures now use the standard syntax.

## Historical failures (fixed on Linux)

| Case | Required behavior | Baseline behavior |
|---|---|---|
| i/class-default-public | Omitted access defaults to public (CIE p. 28). | Rejects the field unless PUBLIC/PRIVATE is explicit. |
| i/identifier-case | Identifier lookup is case insensitive (CIE p. 7). | Reports the differently cased variable as unknown. |
| i/integer-boundaries | The minimum i64 literal is -9223372036854775808. | Silently produces -9223372036854775807; reading the same value through INPUT works. |
| i/standard-while | Accept WHILE condition without DO (CIE p. 21). | Requires DO. |
| iii/argument-count | Wrong argument count, line 4. | Correct cause, no line. |
| iii/builtin-arity | LENGTH arity, line 2. | Correct cause, no line. |
| iii/byref-literal | Non-variable BYREF argument, line 4. | Correct cause, no line. |
| iii/constant-write | Assignment to constant, line 2. | Correct cause, no line. |
| iii/missing-endif | Missing ENDIF at EOF, line 4. | Correct cause, no line. |
| iii/missing-parenthesis | Missing closing parenthesis at EOF, line 3. | Correct cause, no line. |
| iii/missing-then | Missing THEN, line 2. | Correct cause, no line. |
| iii/nominal-type-mismatch | Incompatible named pointers, line 7. | Correct cause, no line. |
| iii/unknown-name | Undeclared name, line 2. | Correct cause, no line. |

At baseline, both native and `-r` paths exposed the four i-group failures. All nine iii-group failures occurred with both native/IR output requests, whether the destination was absent or already contained a sentinel. Failed compilation preserves the destination in these cases. The remaining 32 compiler cases passed locally, including seven groups of 10,000 random records plus 36 boundary records (70,036 total inputs, each checked through native and embedded execution).

Current exact command results are retained in `build/test-results`; the replay uses the original Actions SHA as its seed. Subsequent CI reports use their own explicit push SHA. Platform results from Actions are the authority for Windows/macOS verification; local Linux results do not establish their behavior.

## Coverage gaps

- Not implemented by the compiler, and not required to pass this initial suite: DATE, SET/DEFINE, random files, class methods/constructors/inheritance, INT/RAND, whole-array assignment and INPUT into array/record elements. Rejection of valid CIE syntax must not be relabeled as a successful negative test.
- The guide's p. 14 pointer example points an INTEGER pointer at an enum; it conflicts with the project's explicit strict-pointee contract. Tests use matching pointees rather than silently choosing a new policy.
- LCASE/UCASE are CHAR-to-CHAR in the guide (p. 17), while the project declares STRING results and also supports whole strings. Output observations cover case conversion but do not prove exact static return-type conformance.
- No requirements are invented for unspecified integer overflow, REAL output spelling, out-of-range substring behavior, Unicode character semantics, or uninitialized CIE values. Native C ABI behavior is checked only against that platform's independent C++ reference.
- This is input differential testing of seven fixed programs, not random-program fuzzing. It cannot establish complete parser or optimizer correctness. New bug reports should become small permanent fixed cases.
