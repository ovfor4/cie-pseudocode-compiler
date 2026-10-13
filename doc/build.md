# Building cpsc

Run these commands from the project root unless stated otherwise. For installation and release packaging, see [install.md](install.md).

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

## Manual

The command-line quick reference is in [cpsc.md](cpsc.md). Generating the manual requires Pandoc; ordinary builds do not.

From the project root on Linux:

```bash
sudo apt-get install pandoc
cmake -S . -B build -DCPSC_RUN_MODE=EMBEDDED
make -C build doc
man ./build/doc/cpsc.1
```

You can also run `make doc` inside the configured `build/` directory.
The output is `build/doc/cpsc.1` on Unix and `build/doc/cpsc.html` on Windows,
and is regenerated when the Markdown source changes. On Windows, install Pandoc,
then generate and open the standalone HTML manual from PowerShell:

```powershell
cmake --build build --target doc
Start-Process .\build\doc\cpsc.html
```

The HTML contains its own styles and can be read offline in a browser.
