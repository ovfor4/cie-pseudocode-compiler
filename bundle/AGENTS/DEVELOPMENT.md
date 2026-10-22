# Bundle development

Use Node.js 22+, stable Rust and Tauri 2's native platform prerequisites. On Debian/Ubuntu install `libwebkit2gtk-4.1-dev`, `build-essential`, `libxdo-dev`, `libayatana-appindicator3-dev`, `librsvg2-dev`, `patchelf` and `pkg-config`.

Build the compiler in EMBEDDED mode using the repository build instructions, then run from `bundle/`:

```sh
npm ci --ignore-scripts
npm run prepare:compiler
npm run tauri -- dev
```

`prepare:compiler` copies `../build/cpsc` with the Rust host target suffix needed by Tauri's externalBin packaging on macOS/Linux. On Windows it compresses `cpsc.exe` into an embedded gzip payload. An optional argument selects another compiler executable. Windows uses MSVC Rust for the shell and retains the existing MinGW compiler as a separate process.

For a Linux compilation check use `npm run tauri -- build --no-bundle`. Release packages are a standalone executable on Windows x64 and a .tar.xz containing the .app on macOS arm64. Keep dependency lockfiles committed.

`npm run package` builds the native application and writes `artifacts/cpsc-bundle-YYYYMMDD-HHMMSS-commit-SYSTEM-ARCH.ext`. Timestamp and short SHA come from HEAD, using UTC, as with the existing compiler and plugin. The development version stays unchanged. Windows publishes only the .exe; no installer or adjacent compiler is required at download time. The application extracts the embedded compiler into the current working directory on startup, retains it there, and reports write failures through the UI. macOS builds the .app, retaining its compiler, then archives it with tar and xz. No Linux package is published.

Licenses and notices are included as frontend assets, including inside the single Windows executable, and as app resources on macOS/Linux. The Windows Tauri config disables externalBin and external resources. Windows uses the system WebView2 runtime. `scripts/smoke-package.py` checks a portable Windows executable (including startup extraction), an extracted .app, or Linux's unbundled release directory and executes its compiler without toolchain directories on PATH.

Nightly extends the existing native compiler jobs: reuse each job's EMBEDDED compiler, compile and test the desktop app on Linux, and upload only the Windows/macOS packages. macOS checks the extracted .tar.xz; Windows checks the single .exe in a fresh working directory. Publishing requires the original four packages plus both desktop packages and retains the existing current-SHA guard and asset replacement behavior. Validate workflow changes with actionlint and a fake gh API, never the real nightly release.

The editor imports the plugin's grammar, language configuration and pure CommonJS modules directly. Vite's CommonJS adapter includes those sources. Do not duplicate their rules or add a second tokenizer. VS Code 2026 Dark and its include chain (Dark Modern, Dark+, Dark (Visual Studio)) are pinned under `vendor/vscode/`; retain upstream attribution. All UI colors come from the theme as `--vscode-*` CSS variables set in `editor.js`; never hard-code colors in `style.css`.

Windows and macOS nightly jobs cache Rust dependencies from `src-tauri/target` after selecting the Rust toolchain. The cache includes compiled dependencies and is saved even if a later check fails; project crates are rebuilt so the packaged compiler comes from the current checkout. Rust toolchain and Cargo configuration changes affect the cache key. Release optimization and archive compression settings are unchanged. Compare cold and warm CI runs to measure the benefit; a cache miss must still build normally.

The language editing core (grammar, configuration, indentation, lexical masking and bracket matching) is shared in full. Platform editor API adapters, file dialogs and process management are separate. Keep the plugin free of Bundle runtime dependencies.

The Rust runner launches the packaged compiler directly, using the source directory as cwd. On Windows its absolute path is captured after extracting into the startup working directory; elsewhere it is the sibling `cpsc` executable. Tokio concurrently writes prefilled stdin and drains both output pipes; stdin is dropped after writing. Each window label has its own Runner, so windows run independently; a watch signal kills and reaps the process on Stop, when its window is destroyed, or on application exit. The output Channel sends raw byte chunks (one streaming TextDecoder per stream in the frontend), then a finished event after both pipes reach EOF. EMBEDDED execution disables the host CRT stdout buffer before JIT initialization; Windows JIT lookups prefer the same msvcrt runtime. Do not introduce a shell or pseudo-terminal. The runner passes `-fdiagnostics-color=always`; `src/ansi.js` renders SGR bold and foreground colors from stderr only, so program stdout stays verbatim. Colors use `--vscode-terminal-ansi*`, which fall back to VS Code's terminal dark defaults pinned in `vendor/vscode/terminal-colors.json` because 2026 Dark does not set them.

After building the compiler, run `cargo test --release --manifest-path src-tauri/Cargo.toml`. The two native tests exercise real compilation, prefilled input, EOF, Unicode, diagnostics, output before process exit, stopping, single-run exclusion and restart, including source paths with spaces. CI runs them on all three native platforms.

Keyboard shortcuts are Monaco `KeyMod.CtrlCmd` keybindings: the editor registers Find and Toggle Line Comment, and the document handler matches New, New Window (Shift), Open, Save and Find with `StandardKeyboardEvent.equals`. Never test `ctrlKey`/`metaKey` directly; Monaco resolves CtrlCmd to Cmd on macOS and Ctrl elsewhere.

Use brief English UI labels and messages. Keep one document per window and prefilled stdin. More documents use native OS windows created by the `new_window` command from the configured window; do not add tabs or in-page switching. Before feature commits, run affected plugin tests, build the frontend and check native compilation as appropriate. Do not add a separate UI test framework for routine changes.
