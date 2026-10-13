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

These are build dependencies. All three modes use LLVM to generate IR, so even
`NONE` requires LLVM development libraries when building the compiler.
Users of an `EMBEDDED` executable do not need to install LLVM or `lli`.

Linux
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  llvm-18-dev \
  zlib1g-dev \
  libzstd-dev \
  libtinfo-dev
```

The last three packages supply static support archives for `EMBEDDED` when the
LLVM package uses them. `PATH` additionally needs `lli` at run time, for example
from `llvm-18` with `/usr/lib/llvm-18/bin` in PATH. Clang is only needed if you
want to turn the emitted IR into a native executable yourself.

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

You must explicitly choose `-DCPSC_RUN_MODE=PATH`, `EMBEDDED`, or `NONE` on
initial configuration; there is no default. The choice is saved in CMake's cache.

| Mode | Behavior of `-r` |
|---|---|
| `PATH` | Run external `lli` from PATH; reject `-r` if it is unavailable. |
| `EMBEDDED` | Run with the statically linked LLVM engine inside `cpsc`; no PATH lookup. |
| `NONE` | Reject `-r`; compiling to IR still works. |

Linux
```bash
cmake -S . -B build -DCPSC_RUN_MODE=EMBEDDED \
  -DLLVM_DIR=/usr/lib/llvm-18/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

Distribute `build/cpsc` for `EMBEDDED`. LLVM, the project's libraries and the
compression/terminal support libraries are linked statically; a missing required
archive is a configure error. On Linux with GCC/Clang, the C++ runtime is static
too. The tested Debian/LLVM 18 executable needs only the system libc, libm and
dynamic loader. It runs pseudocode with `-r` as one executable, without LLVM
libraries or `lli` installed. Build for the intended operating system and CPU.

---

mingw-w64 for Windows
```bash
mkdir build && cd build/
export CMAKE_GENERATOR="MinGW Makefiles"
cmake -DCPSC_RUN_MODE=EMBEDDED ..
mingw32-make
```

MinGW uses static linking for `EMBEDDED`; this mode has been verified on Linux,
and the Windows build has not been tested.


## Usage

```bash
cpsc test.cps                 # save test.ir beside the source file
cpsc test.cps -o result.ir    # save IR to the specified path
cpsc test.cps -r             # run using the selected build mode
cpsc test.cps -r -o result.ir # save IR and run it
```

Only one source file is supported. Options may appear before or after it.
With no arguments, `cpsc` exits silently with status 0; help text is planned.
Diagnostics go to stderr. Failed compilation does not write IR; successful
compilation overwrites an existing output file, but cannot overwrite the source.
`-r` returns the program's exit status. `PATH` uses external `lli`; `EMBEDDED`
executes in-process; `NONE` rejects the option. An unavailable runner is rejected
before compiling or writing IR. The running program inherits
stdin/stdout/stderr, so pseudocode `INPUT` reads from the terminal or piped input.
With `-r` alone, no permanent IR is saved (`PATH` deletes its temporary IR;
`EMBEDDED` runs the in-memory module). With `-r -o`, IR is saved before execution
and kept even if execution fails.

## Manual

The command-line quick reference is in `doc/cpsc.md`. Generating the man page requires Pandoc; ordinary builds do not.

From the project root on Linux:

```bash
sudo apt-get install pandoc
cmake -S . -B build -DCPSC_RUN_MODE=EMBEDDED
make -C build doc
man ./build/doc/cpsc.1
```

You can also run `make doc` inside the configured `build/` directory.
The output is `build/doc/cpsc.1` and is regenerated when the Markdown source changes.

# Note

DO NOT OPEN ANY ISSUES IF YOU COMPILE WITH VISUAL STUDIO OR ANYTHING EXCEPT MINGW-W64 (EVEN WITH MINGW-W64, WINDOWS IS LIKELY TO BE A PROBLEM MAKER)

ONLY TESTED ON DEBIAN (AND A BIT MINGW-W64)

Copyright (C) _ov4

This is free software; see the source for copying conditions.  There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

## Contributors

ov4, ChatGPT, Gemini, Claude, et al.
