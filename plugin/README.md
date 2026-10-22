# plugin

By \_ov4.

VS Code support for this project's `.cps` files. Requires VS Code 1.85 or newer and a dark theme.

## Install

Download `cpsc-plugin-latest.vsix` from [Nightly Build](https://github.com/ovfor4/cie-pseudocode-compiler/releases/tag/nightly-build). The same release provides Linux x86_64, Windows x86_64 and macOS 15+ Apple Silicon compiler packages with execution support.

If you installed the old extension (`ovfor4.cie-pseudocode`), uninstall it before installing `ovfor4.plugin`. Update any explicit `editor.defaultFormatter` setting to `ovfor4.plugin`.

In the Extensions panel, choose **... > Install from VSIX...** and select the downloaded `cpsc-plugin-latest.vsix`, or run:

```sh
code --install-extension cpsc-plugin-latest.vsix
```

Open a `.cps` file. The language mode should show **CIE Pseudocode**.

## Features

- C++ style syntax colors that follow your dark theme.
- Automatic Tab indentation and outdent for block keywords.
- **Format Selection**: select code, then press **Cmd+K, Cmd+F** on macOS or **Ctrl+K, Ctrl+F** on Windows/Linux. Only indentation changes.
- Colored `()` and `[]` pairs; unmatched brackets turn red. Quotes and comments are ignored.
- **Run Pseudocode File in Terminal**: click the play button at the top right. The current file is saved, then run with `cpsc ... -r`; selections are ignored. New files ask for a save location first.

Install `cpsc` with execution support and add it to PATH, or set **CIE Pseudocode: Compiler Path** (`cie-pseudocode.compilerPath`) to its full executable path. On Windows, use `cpsc.exe` with PowerShell or Command Prompt; on Linux/macOS, use bash or zsh.

The terminal takes focus so you can answer `INPUT`. Relative file paths use the file's workspace root, or its own directory when no workspace is open. The terminal is reused; stop a running program with **Ctrl+C** before running again.

Use uppercase keywords, `<-` for assignment and `//` for comments. Editor support covers a subset of compiler syntax: CASE and CLASS blocks lack indentation support, string escapes are not recognized, and WHILE indentation still expects DO. These editor limitations do not prevent the compiler from running supported programs.

See [examples/highlighting.cps](examples/highlighting.cps) for an example.
