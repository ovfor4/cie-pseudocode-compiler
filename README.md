# CIE Pseudocode Compiler

A compiler for CIE Pseudocode (A Level 9618 Computer Science)

[Usage quick reference](doc/cpsc.md) · [Build guide](doc/build.md) · [Installation and releases](doc/install.md)

Details can be found here: [Pseudocode Guide for Teachers](https://www.cambridgeinternational.org/Images/697401-2026-pseudocode-guide-for-teachers.pdf)

## Features

- **LLVM compilation**: Generate LLVM IR and run programs when execution is enabled in the build.
- **Runtime checks**: Report array bounds errors, division by zero, file misuse, enum range errors and null pointer dereferences.
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
