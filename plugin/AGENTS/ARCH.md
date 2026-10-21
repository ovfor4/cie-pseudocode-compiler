# Plugin architecture

The extension registers `cie-pseudocode` for `.cps` files. It targets VS Code 1.85+
and dark themes. Runtime code uses CommonJS JavaScript and the VS Code API, with
no npm runtime dependencies. Running files invokes the installed `cpsc` executable;
there is no compiler library or cpptools integration.

## Native editor capabilities and upstream references

- VS Code provides the TextMate engine, theme scope resolution, bracket pair
  colors, automatic bracket closing, formatting commands and keybindings.
- `language-configuration.json` adapts the built-in C++ configuration for CIE's
  comments, brackets, quote pairs and identifiers.
- `syntaxes/cie-pseudocode.tmLanguage.json` adapts small C++ comment, quote and
  comma rules and uses the same general token scope categories. CIE keyword,
  function, type, number and operator patterns are locally defined. Exception keywords
  reuse C++ try/catch/throw scopes, with ENDTRY sharing the TRY scope.
- cpptools was consulted for provider registration. Its formatter and language
  server are not called, imported or bundled. This extension is not a cpptools
  fork. The formatting API accepts locally computed edits; it does not implement
  CIE indentation itself.
- `src/runner.js` adapts Code Runner's terminal creation, display, reuse, close
  handling and Windows shell detection. The editor title button uses the same
  native `$(play)` icon. Upstream revision and notice are in `ThirdPartyNotices.md`.

Native `indentationRules` and `onEnterRules` are not currently configured. The
on-type provider uses the same custom block-depth engine as selection formatting.

References, ignored by Git and excluded from the VSIX:

- `reference/vscode-cpptools/`: shallow clone of
  https://github.com/microsoft/vscode-cpptools
- `reference/vscode-cpp/`: original configuration and grammar from
  https://github.com/microsoft/vscode/tree/main/extensions/cpp

Source attribution is retained in `ThirdPartyNotices.md`.

## Custom CIE behavior

- `src/extension.js` registers range and on-type formatting providers and manages
  invalid bracket decorations for visible editors. Language changes reopen
  documents, so refresh on document open as well as edits and editor changes.
- `src/indentation.js` computes block depth from earlier lines and returns edits
  to leading whitespace only. A selection ending at column zero excludes that
  final line. Formatting is idempotent and preserves line endings and other text.
- `src/lexical.js` masks quotes and comments while preserving UTF-16 offsets.
- `src/brackets.js` uses a stack to report unmatched opening and closing brackets
  and crossed types. Only invalid brackets receive the red decoration; native
  bracket coloring handles valid pairs.
- `src/runner.js` registers `cie-pseudocode.runFile`. It saves the target document,
  uses the new file URI after an untitled save, ignores selections and runs
  `cpsc file -r` in a focused, reusable terminal. Only that terminal's close event
  clears the cached terminal; extension disposal closes it.

## Terminal execution

`cie-pseudocode.compilerPath` is a resource-scoped executable name or full path,
defaulting to `cpsc`. Read it for the saved file. The working directory is that
file's workspace folder, falling back to its containing directory.

Unix commands use POSIX single-quote escaping and `cd ... && ...`. Windows
PowerShell uses literal paths, doubled single quotes, `if ($?)` and the `&` call
operator. Command Prompt uses quoted paths, `cd /d` and `&&`. Windows detection
adapts Code Runner's default-profile logic and also recognizes `pwsh` and custom
profile paths. Cache the shell kind with the terminal, since changing the default
profile does not change an existing terminal. Windows bash profiles and non-POSIX
Unix shells are outside the supported terminal set.

Save cancellation starts nothing. Save or terminal setup failures show an English
error message; compiler and program diagnostics go directly to the terminal.
Terminal execution follows Code Runner's reuse behavior: users must finish or
interrupt an interactive program with Ctrl+C before running another file.

## Language rules and defaults

Use the current compiler as the language reference. Existing editor rules cover
only a subset of its syntax:

- Uppercase, case-sensitive keywords; ASCII `<-` assignment; `//` comments.
- Native C aliases C_INT/C_CHAR/C_STRING/C_DOUBLE and the C_TYPE import keyword
  use type scopes. C_TYPE's quoted expression remains a string, and casts use
  ordinary parentheses. `TYPE Name = C_TYPE("...")` is a single-line alias
  declaration, so it does not open an indentation block.
- The compiler decodes C++17 byte escapes and Unicode escapes in literals; strings
  encode Unicode as UTF-8 and CHAR Unicode escapes must be ASCII. Highlighting
  uses the C++ escape scope and string-suffix scope for adjacent uppercase C.
  Grammar and shared quote masking honor escaped quotes and backslashes; quote
  characters use `'\''`, with no legacy three-quote special case. Neither
  supports multiline literal continuation.
- Blocks: IF/THEN/ELSE/ENDIF, WHILE/DO/ENDWHILE, REPEAT/UNTIL, FOR/NEXT,
  FUNCTION/ENDFUNCTION, PROCEDURE/ENDPROCEDURE, record TYPE/ENDTYPE and
  TRY/CATCH/ENDTRY. CATCH aligns with TRY and starts another indented body;
  ENDTRY closes the block. Bare THROW does not change indentation depth.
- Separate THEN and DO lines activate the pending block once. Enum and pointer
  `TYPE Name = ...` declarations do not open a record block. The compiler accepts
  standard WHILE headers without DO, but the indentation engine still needs DO
  to activate their body.
- Function highlighting accepts leading underscores and explicit, nested template
  arguments. The generic call rule covers the compiler builtins EOF, MALLOC, FREE
  and REALLOC as well as ordinary standard-library functions. Builtin definitions
  live in `lib/CodeGen/Builtins.h`; string functions live in pseudocode-lib.
- Custom type names use C++ type scopes in TYPE/CLASS declarations, variable and
  field declarations, parameter and return types, ARRAY OF, pointer aliases, NEW,
  templates and EXTERN parameter lists. Array-bound contexts shield their colons
  from type annotation rules; CASE OF does not introduce a type. Balanced template
  call lookahead follows Parser::isTemplateCall, requiring a following parenthesis.
  No symbol tracking is performed: custom cast names and ordinary value names
  stay unclassified. Builtin casts retain their existing type scopes.
- A CLASS method context enables the C++ this scope case-insensitively via a
  TextMate injection, including nested declaration expressions. Comments, strings,
  fields outside methods and top-level code do not receive this scope.
- Highlighting covers CASE/ENDCASE/OTHERWISE, CLASS/ENDCLASS/PUBLIC/PRIVATE,
  CONSTANT, TEMPLATE, OVERLOAD/OPERATOR, EXTERN declarations, INCLUDE_OBJ, #LIB
  and ellipses using C++ scopes. OVERLOAD borrows a declaration modifier, #LIB
  borrows #pragma and INCLUDE_OBJ borrows #include. #LIB must occupy its own line.
  These rules do not add indentation support for the corresponding constructs.
  SET, DATE and random-file operations remain unsupported by the compiler.

Language defaults use real Tabs, display width 4, disabled indentation detection,
full automatic indentation, on-type formatting and native bracket pair colors.
The extension name and display name are `plugin`; its ID is `ovfor4.plugin`.
The default formatter is `ovfor4.plugin`. User and workspace settings can
override editor defaults.

The author credit is `_ov4`. The publisher ID stays `ovfor4`, because vsce accepts
only letters, digits and hyphens in publisher IDs. Do not use the author display
name as the extension or formatter ID.
