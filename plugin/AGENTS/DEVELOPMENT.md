# Plugin development

Keep `plugin/README.md` short and focused on installation and use. Put developer
and AI notes here and in `ARCH.md`. All project text must be in English.

## Commands

Use Node.js 22+ for development. From `plugin/`:

```sh
npm ci --ignore-scripts
npm run check
npm test
npm run package
```

`npm run package` runs syntax checks and the existing Node tests, then uses vsce
to generate `plugin-0.1.0.vsix`. Runtime dependencies are not bundled.
`@vscode/vsce`, `vscode-textmate` and `vscode-oniguruma` are development tools only.

Do simple verification before each feature commit and follow the global commit
instructions. Do not introduce a complex test framework for routine changes.

## Verification

The existing tests check real TextMate tokenization, nested block indentation,
selection context, idempotence, bracket errors, terminal command quoting,
save cancellation and terminal lifecycle. Keep Unicode offset tests,
using English text and non-CJK characters such as an emoji.

For changes to editor integration, check in VS Code with a dark theme:

- Open `.cps` and confirm the language mode and syntax colors.
- Press Enter after a block header and type an ending keyword to check Tab depth.
- Format a selection inside a nested block and check its surrounding context.
- Check mixed bracket nesting, unmatched left/right brackets, quotes and comments.
- Repair errors and switch language or files to check decoration cleanup.
- Click the run button on `.cps`; confirm it is absent on other languages.
- Run `OUTPUT` and `INPUT`, including paths with spaces and unsaved changes.
- Read a relative file from the workspace root, then run without a workspace.
- Close the run terminal and run again to check recreation.

Terminal commands have been exercised in isolated VS Code 1.85.2 on Linux
with bash and zsh. Windows PowerShell/cmd command construction is covered by
Node tests; native Windows and macOS execution still needs testing there.

Existing local editor smoke scripts, if present, live under the ignored
`reference/vscode-smoke/` directory. They are not a required build dependency.

## Packaging and installation

`.vscodeignore` excludes tests, agent instructions, dependencies and build
artifacts. Git ignores dependencies, VSIX files and editor test downloads.
Check the generated package contains the runtime files, grammar, language
configuration, concise English README and notices, without `AGENTS/` or tests.

For a local installation, use the Extensions panel's **Install from VSIX...** or:

```sh
code --install-extension plugin-0.1.0.vsix --force
```

In a remote terminal, `code` installs into the connected remote editor. For an
isolated Electron test installation, unset `VSCODE_IPC_HOOK_CLI` and explicitly
set both `--user-data-dir` and `--extensions-dir` on that test editor's CLI.
For GUI tests in this environment, also unset `ELECTRON_RUN_AS_NODE`; a virtual
display may be needed. Avoid changing the user's active editor during isolated
tests.

If user settings disable defaults, inspect the `[cie-pseudocode]` editor settings
declared in `package.json`: real Tabs, tab size 4, automatic indentation, on-type
formatting, the extension as default formatter and enabled bracket pair colors.
