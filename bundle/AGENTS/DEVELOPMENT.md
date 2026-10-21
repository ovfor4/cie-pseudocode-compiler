# Bundle development

Use Node.js 22+, stable Rust and Tauri 2's native platform prerequisites. On Debian/Ubuntu install `libwebkit2gtk-4.1-dev`, `build-essential`, `libxdo-dev`, `libayatana-appindicator3-dev`, `librsvg2-dev`, `patchelf` and `pkg-config`.

Build the compiler in EMBEDDED mode using the repository build instructions, then run from `bundle/`:

```sh
npm ci --ignore-scripts
npm run prepare:compiler
npm run tauri -- dev
```

`prepare:compiler` copies `../build/cpsc` (or `cpsc.exe`) with the Rust host target suffix needed by Tauri's externalBin packaging. An optional argument selects another compiler executable. Windows uses MSVC Rust for the shell and retains the existing MinGW compiler as a separate process.

For a Linux compilation check use `npm run tauri -- build --no-bundle`. Release targets are NSIS on Windows x64 and DMG on macOS arm64. Keep dependency lockfiles committed.

The editor imports the plugin's grammar, language configuration and pure CommonJS modules directly. Vite's CommonJS adapter includes those sources. Do not duplicate their rules or add a second tokenizer. Dark+ and its parent theme are pinned under `vendor/vscode/`; retain upstream attribution.

The language editing core (grammar, configuration, indentation, lexical masking and bracket matching) is shared in full. Platform editor API adapters, file dialogs and process management are separate. Keep the plugin free of Bundle runtime dependencies.

The Rust runner launches the sibling `cpsc` executable directly, using the source directory as cwd. Tokio concurrently writes prefilled stdin and drains both output pipes; stdin is dropped after writing. A watch signal kills and reaps the process on Stop or application exit. The output Channel sends raw byte chunks (one streaming TextDecoder per stream in the frontend), then a finished event after both pipes reach EOF. EMBEDDED execution disables the host CRT stdout buffer before JIT initialization; Windows JIT lookups prefer the same msvcrt runtime. Do not introduce a shell or pseudo-terminal.

After building the compiler, run `cargo test --release --manifest-path src-tauri/Cargo.toml`. The two native tests exercise real compilation, prefilled input, EOF, Unicode, diagnostics, output before process exit, stopping, single-run exclusion and restart, including source paths with spaces. CI runs them on all three native platforms.

Use brief English UI labels and messages. Keep the first version to one document and prefilled stdin. Before feature commits, run affected plugin tests, build the frontend and check native compilation as appropriate. Do not add a separate UI test framework for routine changes.
