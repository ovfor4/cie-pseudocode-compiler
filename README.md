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

# Note

IDK WHAT HAPPENDED IT IS 100% AI-GENERATED DO NOT OPEN ANY ISSUES JUST CLONE AND FIX IT

DO NOT OPEN ANY ISSUES IF YOU COMPILE WITH VISUAL STUDIO OR ANYTHING EXCEPT MINGW-W64 (EVEN WITH MINGW-W64, WINDOWS IS LIKELY TO BE A PROBLEM MAKER)

ONLY TESTED ON DEBIAN (AND A BIT MINGW-W64)

Copyright (C) _ov4

This is free software; see the source for copying conditions.  There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

## Contributors

ov4, ChatGPT, Gemini, Claude, et al.
