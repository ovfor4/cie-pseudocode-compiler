# CIE Pseudocode Compiler

A compiler for CIE Pseudocode (A Level 9618 Computer Science)

[👉 Click to download 👈](https://github.com/ovfor4/cie-pseudocode-compiler/releases/tag/nightly-build)

### TL;DR

[Usage quick reference](doc/cpsc.1.md)

- daily builds, always up to date with updater (Bundle or VSCode plugin only)
- no external dependencies, just single file
- easy to use, with colorful & detail diagnostic
- easy to deploy massively (helpful for schools)

Here's the "standard": [Pseudocode Guide for Teachers](https://www.cambridgeinternational.org/Images/697401-2026-pseudocode-guide-for-teachers.pdf), and [Standard Library Functions (aka insert)](https://bestexamhelp.com/exam/cambridge-international-a-level/computer-science-9618/2025/9618_w25_in_21.pdf)

---

## Nerdy info

- [Build guide](doc/build.md)
- [Installation and releases](doc/install.md)

#### Features

- **LLVM compilation**: Generate native executables by default, save LLVM IR with `-S`, or run programs with `-r` when execution is enabled in the build. Default and temporary IR files use `.ll`.
- **Runtime checks**: Throw exceptions for array bounds errors, division by zero, file misuse, enum range errors and null pointer dereferences.
- **Exception handling**: Catch exceptions with stack unwinding using the compiler extensions [TRY/CATCH/ENDTRY and THROW](doc/language/keyword/cpsc-keyword-try.7.md).
- **Arrays**: Support multiple dimensions and custom lower and upper bounds.
- **Strings**: Provide the owning STRING class and string functions through an embedded standard library, loaded and linked by default; use `-no-std` to opt out.
- **Functions and procedures**: Pass values, arrays and records by value; procedures also support `BYREF` parameters.
- **Text files**: Support `OPENFILE`, `READFILE`, `WRITEFILE`, `CLOSEFILE` and `EOF`.
- **User-defined types**: Support enumerated types, pointers and nested records.
- **Classes**: Support public/private fields and methods, constructors, destructors, copying and operator overloads.
- **Templates**: Instantiate function, procedure and class [templates](doc/language/keyword/cpsc-keyword-template.7.md) with type deduction, explicit type arguments and variadic packs.
- **Selection and constants**: Support `CASE OF` branches and named literal `CONSTANT` values.

### TODO

- **Source TODOs**: Resolve remaining `TODO` comments.
- **Random files**: Add `OPENFILE FOR RANDOM`, `SEEK`, `GETRECORD` and `PUTRECORD`.
- **Set types**: Implement `SET` and `DEFINE`.
- **Other syntax**: Add `DATE`.

### Note

Vibe project. If you have any problems, please open issues. If you want to contribute to this project, please open PR (we never use dual-license because it's scam).

If you have problems configuring the compiler, please open an issue and leave your eamail or discord, and I will contact you as soon as possible (support is 100% free).

### Legal info

License: AGPL 3.0 or later.

Copyright (C) _ov4

This is free software; see the source for copying conditions.  There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

### Contributors

ov4, Claude, ChatGPT, Gemini, et al.
