# Build dependencies and toolchain

`doc/build.md` recommends which packages to install and which to download;
CMake does not enforce that split. The modular `cmake/LocalDependencies.cmake`
entry point extracts Debian, MSYS2 and Homebrew binary packages from the build
directory's `deps/packages/` into `deps/root/`. Standard bin/lib/include layouts
under the build directory are also searched. The shared
`cmake/FindDependency.cmake` helper searches locally first, then the system, and
reports missing required dependencies. Compilers, make, LLVM, support libraries,
Pandoc, Git and installer/relocation tools use the same lookup policy. System-only
and mixed installations are supported. Local dependencies override cached system
paths except for supplied compilers. Cached or explicit CMake compiler paths and
initial CC/CXX selections are preserved and validated; default discovery prefers
clang-23/clang++-23, then unversioned clang/clang++. Build tools are resolved before
`project()` enables compiler checks; compiler information is refreshed when the
selected executable changes.
Platform modules relocate package paths only inside the extraction directory;
Homebrew bottles also receive local Cellar/opt links and Mach-O relocation/signing.
Package hashes skip unchanged extractions; a changed package set rebuilds the
managed root. Downloads belong to the explicit dependency commands and static LLD
helper in `doc/build.md`, not to CMake. The Windows Pandoc ZIP is downloaded directly from its official
release because MSYS2 has no Pandoc package.


Windows build instructions retain MSYS2 MINGW64. Its LLVM/Clang package names
and compiler executable names are unversioned; CMake still requires major 23.
The build guide explicitly selects `/mingw64/bin/clang.exe` and `clang++.exe`
through `cygpath -m`. MINGW64 retains the GNU C++ runtime; cpsc links with LLD. Switching to
CLANG64 would change the C/C++ runtime and is a separate task.

Build pin: **LLVM 23 and upstream Clang 23** (any 23.x release); 23.1.3 is verified
on Linux. LLVM 23 uses `CreateGlobalString` for opaque string pointers,
`hasTerminator()` for block termination checks (its `getTerminator()` requires an
existing terminator), and an explicit `llvm::Triple` for the embedded target.
