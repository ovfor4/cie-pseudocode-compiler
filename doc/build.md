# Building cpsc

Run these commands from the project root. For installation and release packaging,
see [install.md](install.md).

## Dependencies

The commands below recommend installing CMake, make and Clang as system tools
and downloading LLVM and other dependencies to `build/deps/packages/`. CMake
extracts downloaded packages into `build/deps/root/` during configuration.

These are setup recommendations, not requirements on where dependencies must
be installed. Once CMake itself can run, compilers, build tools, libraries and
optional tools all follow the same lookup rule: local build directories first,
then the system. A required dependency missing from both locations is an error.
System-only and mixed installations are supported, including system-installed
LLVM without any local LLVM download. Local dependencies take precedence over
previously cached system paths.

CMake searches standard `bin/`, `lib/` and `include/` directories under the build
directory as well as the extracted package layouts. Compiler and make selection
follow the same rule as LLVM and Pandoc selection.

Unchanged packages are not extracted again. Keep one version of each package in
the download directory; remove old archives before downloading a different
version. CMake rebuilds its extraction directory when the package set changes.
If you use a different CMake build directory, download into its `deps/packages/`
instead. Removing the build directory also removes its downloaded dependencies.

All three execution modes need LLVM development libraries to build, including
`NONE`. Users of an `EMBEDDED` executable do not need LLVM or `lli`.
LLVM 18 is the tested version; newer versions remain untested.

### Linux (Debian/Ubuntu)

Use a distribution repository that provides LLVM 18. Recommended build tools:

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends build-essential cmake clang-18
```

Recommended LLVM and support-package downloads:

```bash
mkdir -p build/deps/packages
(
  cd build/deps/packages
  apt-get download \
    llvm-18-dev llvm-18 llvm-18-runtime llvm-18-tools llvm-18-linker-tools \
    libllvm18 libclang-cpp18 libz3-dev libz3-4 \
    libffi-dev libffi8 libxml2-dev libxml2 \
    zlib1g-dev zlib1g libzstd-dev libzstd1 libncurses-dev libtinfo6
)
```

`apt-get download` also downloads explicitly named packages that are already
installed. The LLVM companion packages satisfy its CMake imports, while the
development support packages supply the static archives used by `EMBEDDED`.

### Windows (MSYS2 MINGW64)

Run these commands in the MSYS2 **MINGW64** shell. Update MSYS2 first, reopening
the shell and repeating the update if requested:

```bash
pacman -Syu
pacman -S --needed \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-make \
  mingw-w64-x86_64-clang
```

Recommended dependency downloads to the project build directory:

```bash
mkdir -p build/deps/packages
pacman -Sw --noconfirm --cachedir "$PWD/build/deps/packages" \
  mingw-w64-x86_64-llvm \
  mingw-w64-x86_64-llvm-libs \
  mingw-w64-x86_64-llvm-tools \
  mingw-w64-x86_64-lld \
  mingw-w64-x86_64-libffi \
  mingw-w64-x86_64-libxml2 \
  mingw-w64-x86_64-libiconv \
  mingw-w64-x86_64-zlib \
  mingw-w64-x86_64-zstd
```

`-Sw` downloads packages without installing them; omit `--needed` here so that
already installed dependencies are also downloaded. CMake keeps the packages'
`mingw64/` layout under `build/deps/root/`. MSYS2 supplies its current LLVM
version. Windows builds have only been reviewed statically.

### macOS (Homebrew)

Use Clang and make from the Xcode Command Line Tools. If they are missing, run
`xcode-select --install`. Install CMake with Homebrew:

```bash
brew install cmake
```

Recommended binary-bottle downloads to the build directory:

```bash
mkdir -p build/deps/packages
HOMEBREW_CACHE="$PWD/build/deps/packages" brew fetch --force-bottle \
  llvm@18 zlib zstd ncurses lz4 xz
```

This requires binary bottles available for your macOS version and CPU; source
archives are not used. CMake extracts the bottles into a local `Cellar/`, creates
local `opt/` links and relocates their CMake and dynamic-library paths using the
Command Line Tools. The macOS build has only been reviewed statically.

## Build

You must explicitly choose `-DCPSC_RUN_MODE=PATH`, `EMBEDDED`, or `NONE` on
initial configuration; there is no default. The choice is saved in CMake's cache.

| Mode | Behavior of `-r` |
|---|---|
| `PATH` | Run external `lli` from PATH; reject `-r` if it is unavailable. |
| `EMBEDDED` | Run with the statically linked LLVM engine inside `cpsc`; no PATH lookup. |
| `NONE` | Reject `-r`; compiling to IR still works. |

Linux:

```bash
cmake -S . -B build -DCPSC_RUN_MODE=EMBEDDED -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18
cmake --build build -j4
```

MSYS2 MINGW64:

```bash
cmake -S . -B build -G "MinGW Makefiles" \
  -DCPSC_RUN_MODE=EMBEDDED -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
mingw32-make -C build -j4
```

macOS:

```bash
cmake -S . -B build -DCPSC_RUN_MODE=EMBEDDED -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
cmake --build build -j4
```

If LLVM and its required support libraries are already installed in the system,
skip the download steps and use the same build commands. Local and system
dependencies can also be mixed; the lookup rule applies to each dependency.
Local packages do not need `-DLLVM_DIR`. You can specify
`-DLLVM_DIR=/path/to/lib/cmake/llvm` for a system installation outside the usual
search paths or to select a system LLVM version when several are installed.

Distribute `build/cpsc` (Windows: `build/cpsc.exe`) for `EMBEDDED`. LLVM and its
required compression/terminal support libraries are linked statically; a missing
required archive is a configure error. Linux also links the C++ runtime statically,
and MinGW uses static linking. The tested Debian/LLVM 18 executable needs only
the system libc, libm and dynamic loader. Build for the intended OS and CPU.

For `PATH` mode, add the extracted `lli` directory to your shell's PATH after
configuration. On Linux, also expose its downloaded runtime libraries:

```bash
export PATH="$PWD/build/deps/root/usr/lib/llvm-18/bin:$PATH"
export LD_LIBRARY_PATH="$PWD/build/deps/root/usr/lib/$(cc -print-multiarch)${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

MSYS2 MINGW64:

```bash
export PATH="$PWD/build/deps/root/mingw64/bin:$PATH"
```

macOS:

```bash
export PATH="$PWD/build/deps/root/opt/llvm@18/bin:$PATH"
```

## Manual

The command-line reference is in [cpsc.1.md](cpsc.1.md); the
[language overview](language/cpsc-language-overview.7.md) links to the syntax manuals. Pandoc is required only for the
`doc`, installation and release targets. Recommended local downloads follow;
an existing system Pandoc works with the same manual-generation command.

Linux:

```bash
(
  cd build/deps/packages
  apt-get download pandoc pandoc-data \
    libffi8 libgmp10 liblua5.4-0 libnuma1 libyaml-0-2 zlib1g
)
```

macOS:

```bash
HOMEBREW_CACHE="$PWD/build/deps/packages" brew fetch --force-bottle pandoc gmp
```

Windows has no MSYS2 Pandoc package. Download the official x86_64 ZIP from
PowerShell, without running an installer:

```powershell
New-Item -ItemType Directory -Force build/deps/packages | Out-Null
$release = Invoke-RestMethod https://api.github.com/repos/jgm/pandoc/releases/latest
$asset = $release.assets | Where-Object { $_.name -like 'pandoc-*-windows-x86_64.zip' }
Invoke-WebRequest -UseBasicParsing $asset.browser_download_url -OutFile "build/deps/packages/$($asset.name)"
```

The ZIP is provided on the [official Pandoc download page](https://pandoc.org/installing.html).

Reconfigure using your platform's build command above, then generate the manual:

```bash
cmake --build build --target doc
```

CMake selects local Pandoc first, sets its local runtime/data paths as needed,
and falls back to Pandoc in the system PATH. The outputs are `build/doc/cpsc.1` and `build/doc/language/**/*.7` on Unix,
or `build/doc/cpsc.html` and `build/doc/language/**/*.html` on Windows. Each page
is regenerated when its source, rendering configuration or downloaded packages
change. New language pages are discovered automatically during the build.

On Unix, open the command manual with `man ./build/doc/cpsc.1`, or a syntax
page with `man ./build/doc/language/keyword/cpsc-keyword-if.7`. On Windows, open the standalone,
offline HTML from PowerShell:

```powershell
Start-Process .\build\doc\cpsc.html
```
