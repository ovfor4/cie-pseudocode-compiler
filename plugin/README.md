# CIE Pseudocode

By \_ov4.

VS Code support for this project's `.cps` files. Requires VS Code 1.85 or newer and a dark theme.

## Install

Download the VSIX from [Nightly Build](https://github.com/ovfor4/cie-pseudocode-compiler/releases/tag/nightly-build). The same release provides the Linux x86_64 compiler with execution support.

In the Extensions panel, choose **... > Install from VSIX...** and select `cie-pseudocode-0.1.0.vsix`, or run:

```sh
code --install-extension cie-pseudocode-0.1.0.vsix
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

Use uppercase keywords, `<-` for assignment and `//` for comments. Support matches the current compiler.

See [examples/highlighting.cps](examples/highlighting.cps) for an example.
