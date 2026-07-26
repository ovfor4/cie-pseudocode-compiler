# CIE Pseudocode Compiler

A compiler for CIE Pseudocode (A Level 9618 Computer Science)

Details can be found here: [Pseudocode Guide for Teachers](https://www.cambridgeinternational.org/Images/697401-2026-pseudocode-guide-for-teachers.pdf)

IDK WHAT HAPPENDED IT IS 100% AI-GENERATED DO NOT OPEN ANY ISSUES JUST CLONE AND FIX IT

## Features

- use LLVM, perhaps support all archs
- built-in runtime checking to prevent illegal operations (eg. array, integer division), and prevent kernal to kill the program immediately
- no Segmentation Fault (Core Dumped), beginner-friendly
- array flatting with row-major (the standard of the industry), with `LowerBounds`, `UpperBounds`, etc. support any dimension with any lower/upper bounds
- cache-friendly array (perhaps)
- dynamic string handling, with full implementation of all CIE string functions, with high security
- native support for BYREF/BYVAL using pointers
- text file handling (OPENFILE/READFILE/WRITEFILE/CLOSEFILE/EOF) with runtime misuse
  checking (wrong mode, not open, read past EOF, ... all die with a clean `[Fatal]`
  message instead of corrupting anything)
- user-defined types (CIE §4): enumerated types (with runtime range checking on
  `enum ± INTEGER`), pointers (`^x` / `p^`, strict nominal typing, null-deref dies
  with `[Fatal]` instead of segfaulting — linked lists work, `TYPE NodePtr = ^Node`
  may forward-reference), records (nested, arrays of records, `Form[i].Field <- x`)
- whole arrays and records as FUNCTION/PROCEDURE parameters (`ARRAY OF T` or
  `ARRAY[l:u,...] OF T` as a checked contract; BYVAL really copies), with the
  spec-correct sticky BYREF/BYVAL and BYREF-is-procedure-only rules from §8.3
- ultra-fast compiling & executing

## TODO

- search for `TODO` in my code
- random files (OPENFILE FOR RANDOM / SEEK / GETRECORD / PUTRECORD) — `type` is done, unblocked
- SET types (`SET` / `DEFINE` are reserved and give a clear "not implemented" error)
- CASE OF, CONSTANT, DATE


## Dependencies

Linux
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  llvm-18-dev \
  llvm \
  clang \
  lld
```

---

mingw-w64 for Windows
```bash
pacman -Syu
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-llvm \
  mingw-w64-x86_64-clang \
  mingw-w64-x86_64-lld \
  mingw-w64-x86_64-make
```

## Build

Linux
```bash
mkdir build && cd build/
cmake ..
make
```

---

mingw-w64 for Windows
```bash
mkdir build && cd build/
export CMAKE_GENERATOR="MinGW Makefiles"
cmake ..
mingw32-make
```


# Note

DO NOT OPEN ANY ISSUES IF YOU COMPILE WITH VISUAL STUDIO OR ANYTHING EXCEPT MINGW-W64 (EVEN WITH MINGW-W64, WINDOWS IS LIKELY TO BE A PROBLEM MAKER)

ONLY TESTED ON DEBIAN (AND A BIT MINGW-W64)

Copyright (C) _ov4

This is free software; see the source for copying conditions.  There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

## Contributors

ov4, ChatGPT, Gemini, Claude, et al.
