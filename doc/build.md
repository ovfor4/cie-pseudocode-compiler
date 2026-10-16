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
previously cached system paths, except for an explicitly selected compiler.

CMake searches standard `bin/`, `lib/` and `include/` directories under the build
directory as well as the extracted package layouts. Compiler and make selection
follow the same rule as LLVM and Pandoc selection unless a compiler is supplied
through `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER` (including cached values), or
the initial `CC`/`CXX` environment. Supplied compilers are preserved and checked.
Default discovery prefers `clang-23` and `clang++-23`, then `clang` and `clang++`.

Unchanged packages are not extracted again. Keep one version of each package in
the download directory; remove old archives before downloading a different
version. CMake rebuilds its extraction directory when the package set changes.
If you use a different CMake build directory, download into its `deps/packages/`
instead. Removing the build directory also removes its downloaded dependencies.

All three execution modes need LLVM, Clang and LLD 23 development libraries, Python 3.9 or newer, and the native C/C++ development libraries to build, including `NONE`. Users of an `EMBEDDED` executable do not need LLVM or `lli`.
CMake strictly requires LLVM development libraries and both C/C++ compilers to
have major version 23. The compilers must be upstream Clang; GCC and AppleClang
are rejected. Any 23.x release is accepted; 22 and 24 are rejected. LLVM/Clang
23.1.3 is verified on Linux; Windows and macOS remain unverified.

When upgrading an existing build, replace its old LLVM packages and explicitly
select Clang 23 for both cached compiler paths, or use a fresh build directory.

### Linux (Debian/Ubuntu)

Use a distribution repository that provides LLVM 23. Recommended build tools:

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends build-essential cmake clang-23 lld-23 python3
```

Recommended LLVM and support-package downloads:

```bash
mkdir -p build/deps/packages
(
  cd build/deps/packages
  apt-get download \
    llvm-23-dev llvm-23 llvm-23-runtime llvm-23-tools llvm-23-linker-tools \
    libllvm23 libclang-cpp23 libclang-23-dev liblld-23-dev liblld-23 libpolly-23-dev libz3-dev libz3-4 \
    libffi-dev libffi8 libxml2-dev libxml2 \
    zlib1g-dev zlib1g libzstd-dev libzstd1 libncurses-dev libtinfo6
)
```

`apt-get download` also downloads explicitly named packages that are already
installed. The LLVM companion packages satisfy its CMake imports, while the
development support packages supply required static archives. Clang, LLD and Polly
archives are needed for embedded native linking in every execution mode.

### Windows (MSYS2 MINGW64)

Run these commands in the MSYS2 **MINGW64** shell. Update MSYS2 first, reopening
the shell and repeating the update if requested:

```bash
pacman -Syu
pacman -S --needed \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-make \
  mingw-w64-x86_64-clang \
  mingw-w64-x86_64-clang-tools-extra \
  mingw-w64-x86_64-python
```

Confirm that both installed compilers report major version 23:

```bash
/mingw64/bin/clang.exe --version
/mingw64/bin/clang++.exe --version
```

The [MINGW64 Clang package](https://packages.msys2.org/packages/mingw-w64-x86_64-clang)
and [LLVM package](https://packages.msys2.org/packages/mingw-w64-x86_64-llvm)
currently provide 23.1.3. MSYS2 keeps these package names unversioned; its
compiler executables are `clang.exe` and `clang++.exe`, not `clang-23.exe` or
`clang++-23.exe`. CMake rejects another major version even when the package
name is unchanged. MINGW64 Clang defaults to the GNU linker and C++ library supplied by its package dependencies. cpsc is compiled with Clang 23 and linked with LLD, retaining the GNU C++ runtime.

Before downloading LLVM, confirm that these repository packages' `Version`
fields all start with `23.`:

```bash
pacman -Si mingw-w64-x86_64-llvm \
  mingw-w64-x86_64-llvm-libs mingw-w64-x86_64-llvm-tools
```

Recommended dependency downloads to the project build directory:

```bash
mkdir -p build/deps/packages
pacman -Sw --noconfirm --cachedir "$PWD/build/deps/packages" \
  mingw-w64-x86_64-clang \
  mingw-w64-x86_64-clang-tools-extra \
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
version; check that it is 23. Windows builds have only been reviewed statically.

The `clang-tools-extra` package is required because MSYS2's `ClangTargets.cmake` checks the extra-tool archives, including `libclangApplyReplacements.a`, when loading the Clang development package, even though cpsc does not invoke those tools.

### macOS (Homebrew)

Install the Xcode Command Line Tools for the macOS SDK, make and package
relocation tools with `xcode-select --install` if needed. The LLVM bottle below
provides Clang 23. Install the build tools and LLVM SDK with Homebrew:

```bash
brew install cmake python llvm@23
```

Recommended binary-bottle downloads to the build directory:

```bash
mkdir -p build/deps/packages
HOMEBREW_CACHE="$PWD/build/deps/packages" brew fetch --force-bottle \
  llvm zlib zstd ncurses lz4 xz
```

[Homebrew's `llvm` formula](https://formulae.brew.sh/formula/llvm) currently
provides 23; verify the downloaded version before configuring. This requires
binary bottles available for your macOS version and CPU. CMake extracts the bottles into a local `Cellar/`, creates
local `opt/` links and relocates their CMake and dynamic-library paths using the
Command Line Tools. The macOS build has only been reviewed statically.

Homebrew's `lld` formula builds shared libraries, which cannot be embedded in the single-file compiler. Before configuring cpsc, build static LLD against the LLVM version selected for the compiler:

```bash
sh tools/build-static-lld.sh "$(brew --prefix llvm@23)" "$PWD/build" 3
```

The helper downloads the matching official LLVM source release, extracts LLD, its common CMake utilities and libunwind's Mach-O headers, and builds LLD using the installed LLVM SDK. It installs static LLD libraries, headers, tools and CMake configuration into `build/`, where cpsc's local dependency lookup finds them before system packages. It does not rebuild LLVM or libunwind. Use the same build directory for the helper and cpsc. For an already-extracted LLVM bottle, pass its `build/deps/root/opt/llvm` prefix instead; the LLVM versions must match. The helper uses the host architecture and defaults to a macOS 15.0 deployment target; set `MACOSX_DEPLOYMENT_TARGET` to override that target for both builds.

## Build

You must explicitly choose `-DCPSC_RUN_MODE=PATH`, `EMBEDDED`, or `NONE` on
initial configuration; there is no default. The choice is saved in CMake's cache.

| Mode | Behavior of `-r` |
|---|---|
| `PATH` | Run external `lli` from LLVM 23 in PATH; reject `-r` before compilation if it is missing or its version cannot be validated. |
| `EMBEDDED` | Run with the statically linked LLVM engine inside `cpsc`; no PATH lookup. |
| `NONE` | Reject `-r`; native and IR compilation still work. |

Linux:

```bash
cmake -S . -B build -DCPSC_RUN_MODE=EMBEDDED -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang-23 -DCMAKE_CXX_COMPILER=clang++-23
cmake --build build -j4
```

MSYS2 MINGW64:

```bash
cmake -S . -B build -G "MinGW Makefiles" \
  -DCPSC_RUN_MODE=EMBEDDED -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$(cygpath -m /mingw64/bin/clang.exe)" \
  -DCMAKE_CXX_COMPILER="$(cygpath -m /mingw64/bin/clang++.exe)"
mingw32-make -C build -j4
```

These explicit paths select the MINGW64 compilers checked above. `cygpath -m`
converts the MSYS2 paths for native Windows CMake. Configuration must report
Clang 23 for both compilers and `Found LLVM 23.x` for the selected development
libraries. Use the MINGW64 packages throughout; do not mix CLANG64 or UCRT64
libraries into this build.

macOS:

```bash
cmake -S . -B build -DCPSC_RUN_MODE=EMBEDDED -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$PWD/build/deps/root/opt/llvm/bin/clang" \
  -DCMAKE_CXX_COMPILER="$PWD/build/deps/root/opt/llvm/bin/clang++"
cmake --build build -j4
```

If LLVM and its required support libraries are already installed in the system,
skip the download steps and use the same build commands. Local and system
dependencies can also be mixed; the lookup rule applies to each dependency.
Local packages do not need `-DLLVM_DIR`. You can specify
`-DLLVM_DIR=/path/to/lib/cmake/llvm` for a system installation outside the usual
search paths or to select LLVM 23 when several system versions are installed.
For a system Homebrew installation, select the compilers under
`$(brew --prefix llvm)/bin` instead of the extracted paths above.

Distribute `build/cpsc` (Windows: `build/cpsc.exe`) for `EMBEDDED`. LLVM and its
required compression/terminal support libraries are linked statically; a missing
required archive is a configure error. Linux also links the C++ runtime statically,
and MinGW uses static linking. The tested Debian/LLVM 23 executable needs only
the system libc, libm and dynamic loader. Build for the intended OS and CPU.

All execution modes embed the Clang driver, LLD and the startup objects, runtime archives and system-library link inputs needed for native output. Python prepares these compressed resources during configuration; it is not needed by users. Native output requires no installed compiler, linker or development SDK. On Linux, generated programs statically link the C++ runtime and dynamically link glibc and its system libraries, so they require a compatible system glibc. Do not use global `-static` on Linux. macOS embeds SDK library stubs and uses operating-system runtime libraries; Windows embeds MinGW link inputs and statically links non-system runtimes.

For `PATH` mode, add the extracted LLVM 23 `lli` directory to your shell's PATH
after configuration. Only `-r` checks the selected `lli --version`, with a
five-second timeout. Missing, wrong or unrecognized versions fail before source
compilation or IR output. Compiling without `-r` does not check external tools;
execution does not invoke or check `clang`/`clang++`. On Linux, also expose its
downloaded runtime libraries:

```bash
export PATH="$PWD/build/deps/root/usr/lib/llvm-23/bin:$PATH"
export LD_LIBRARY_PATH="$PWD/build/deps/root/usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

MSYS2 MINGW64:

```bash
export PATH="$PWD/build/deps/root/mingw64/bin:$PATH"
```

In this shell, `lli.exe --version` should identify LLVM 23. `cpsc -r` validates
the same PATH executable automatically before compiling the source. When
running from PowerShell or Command Prompt, add the Windows path to this `bin`
directory to that terminal's PATH so its DLLs can also be found.

macOS:

```bash
export PATH="$PWD/build/deps/root/opt/llvm/bin:$PATH"
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

Windows has no MSYS2 Pandoc package. After configuring the build above, run
these commands in the MSYS2 MINGW64 shell to download the official x86_64 ZIP
and generate the manual without running an installer:

```bash
mkdir -p build/deps/packages
wget -O build/deps/packages/pandoc-3.12.1-windows-x86_64.zip \
  https://github.com/jgm/pandoc/releases/download/3.12.1/pandoc-3.12.1-windows-x86_64.zip
cmake -S . -B build
mingw32-make -C build doc
```

CMake extracts the ZIP into `build/deps/root/` during reconfiguration and finds
the local Pandoc automatically.

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
