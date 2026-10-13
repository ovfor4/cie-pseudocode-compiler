# CIE Pseudocode

VS Code support for this project's `.cps` files. Requires VS Code 1.85 or newer and a dark theme.

## Install

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

Use uppercase keywords, `<-` for assignment and `//` for comments. Support matches the current compiler. Compile and run with `cpsc` separately.

See [examples/highlighting.cps](examples/highlighting.cps) for an example.
