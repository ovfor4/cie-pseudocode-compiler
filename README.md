# CIE Pseudocode Compiler

A compiler for CIE Pseudocode (A Level 9618 Computer Science)

[Usage quick reference](doc/cpsc.1.md) · [Build guide](doc/build.md) · [Installation and releases](doc/install.md)

Download the Linux x86_64, Windows x86_64 or macOS 15+ Apple Silicon compiler package and VS Code extension from [Nightly Build](https://github.com/ovfor4/cie-pseudocode-compiler/releases/tag/nightly-build). The compiler uses EMBEDDED mode and needs no LLVM installation. Each push to `main` removes the previous attachments before rebuilding all four packages; each package appears when its build and checks finish.

Details can be found here: [Pseudocode Guide for Teachers](https://www.cambridgeinternational.org/Images/697401-2026-pseudocode-guide-for-teachers.pdf)

## Features

- **LLVM compilation**: Generate native executables by default, save LLVM IR with `-S`, or run programs with `-r` when execution is enabled in the build. Default and temporary IR files use `.ll`.
- **Runtime checks**: Throw exceptions for array bounds errors, division by zero, file misuse, enum range errors and null pointer dereferences.
- **Exception handling**: Catch exceptions with stack unwinding using the compiler extensions [TRY/CATCH/ENDTRY and THROW](doc/language/keyword/cpsc-keyword-try.7.md).
- **Arrays**: Support multiple dimensions and custom lower and upper bounds.
- **Strings**: Support dynamic strings and built-in string functions.
- **Functions and procedures**: Pass values, arrays and records by value; procedures also support `BYREF` parameters.
- **Text files**: Support `OPENFILE`, `READFILE`, `WRITEFILE`, `CLOSEFILE` and `EOF`.
- **User-defined types**: Support enumerated types, pointers and nested records.

## TODO

- **Source TODOs**: Resolve remaining `TODO` comments.
- **Random files**: Add `OPENFILE FOR RANDOM`, `SEEK`, `GETRECORD` and `PUTRECORD`.
- **Set types**: Implement `SET` and `DEFINE`.
- **Other syntax**: Add `CASE OF`, `CONSTANT` and `DATE`.

## Note

Vibe project. If you have any problems, please open issues. If you want to contribute to this project, please open PR (we never use dual-license because it's scam).

If you have problems configuring the compiler, please open an issue and leave your eamail or discord, and I will contact you as soon as possible (support is 100% free).

## Legal info

License: AGPL 3.0 or later.

Copyright (C) _ov4

This is free software; see the source for copying conditions.  There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

## Contributors

ov4, Claude, ChatGPT, Gemini, et al.
