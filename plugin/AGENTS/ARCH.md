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
  builtin, number and operator patterns are locally defined.
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

Follow the current compiler, rather than implementing every PDF feature:

- Uppercase, case-sensitive keywords; ASCII `<-` assignment; `//` comments.
- Double-quoted strings and single-quoted characters have no escapes or multiline
  continuation. The compiler accepts `'''` as a quote character literal.
- Blocks: IF/THEN/ELSE/ENDIF, WHILE/DO/ENDWHILE, REPEAT/UNTIL, FOR/NEXT,
  FUNCTION/ENDFUNCTION, PROCEDURE/ENDPROCEDURE and record TYPE/ENDTYPE.
- Separate THEN and DO lines activate the pending block once. Enum and pointer
  `TYPE Name = ...` declarations do not open a record block.
- Builtins: LENGTH, MID, RIGHT, LEFT, LCASE, UCASE, ASC, CHR, IS_NUM, NUM_TO_STR,
  STR_TO_NUM and EOF. See the table in `lib/CodeGen/CodeGen.cc`.
- CASE, classes, SET, DATE and random-file operations are outside the current
  plugin scope. Do not add them without an explicit scope change.

Language defaults use real Tabs, display width 4, disabled indentation detection,
full automatic indentation, on-type formatting and native bracket pair colors.
The default formatter is `ovfor4.cie-pseudocode`. User and workspace settings can
override editor defaults.

The author credit is `_ov4`. The publisher ID stays `ovfor4`, because vsce accepts
only letters, digits and hyphens in publisher IDs. Do not use the author display
name as the extension or formatter ID.
