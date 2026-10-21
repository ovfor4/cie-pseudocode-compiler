# Static linking and native compilation

All modes require static LLVM, Clang Driver and LLD archives. `cmake/EmbeddedLLVM.cmake` also selects
static compression/terminal support archives when LLVM uses them. Linux Clang
builds link the C++ runtime statically; MinGW uses `-static`. The tested Debian
LLVM 23 executable has only libc, libm and the system loader as dynamic dependencies.
macOS links with `-dead_strip_dylibs` to omit unused dylibs inherited
from LLVM's support interface, including Homebrew's Z3. Keep the nightly package's
non-system dependency check ([NIGHTLY.md](NIGHTLY.md)); required dylibs are not removed by this option.
On MinGW, `LLVMWindowsManifest` explicitly uses static libxml2, iconv and zlib
plus the system bcrypt library. XML definitions are loaded with `--whole-archive`
to satisfy dllimport references in the prebuilt LLVM archive. `-static` alone
does not replace an imported target's explicit `.dll.a` path.

`NativeCompiler` clones the module, sets the host triple/DataLayout and emits a PIC
object. Clang Driver constructs one platform linker job, which is dispatched to
embedded LLD without launching a process. `cmake/NativeCompiler.cmake` runs
`tools/driver/prepare-native-runtime.pl` at configure time to capture the build
toolchain's link inputs and compress them into generated C++ data. Native compilation
extracts startup objects, runtime archives and system link interfaces to a temporary
directory; GNU linker scripts are relocated. macOS embeds SDK library stubs, not
headers or system runtime dylibs. No user development toolchain is required.
Linux statically links non-system runtimes but dynamically links glibc; do not use
global `-static` there. Generated binaries require a compatible host system runtime.

Homebrew's LLD package contains shared libraries. Run `tools/build-static-lld.sh`
with the selected LLVM prefix and cpsc build directory before configuring on macOS.
It builds matching LLD sources, installs static archives and tools into the build
prefix, and leaves LLVM itself installed. Source and build caches are versioned;
the resulting local `lib/cmake/lld` takes precedence over shared system packages.
The helper's build-time linker can use the installed LLVM dylib; cpsc still links
the LLD archives and static LLVM components.
Both macOS builds (the LLD helper and cpsc) explicitly select the SDK with `CMAKE_OSX_SYSROOT`
before compiler detection, so imported SDK include paths do not precede libc++.
The nightly macOS job's helper caching and LLVM bottle checks are described in
[NIGHTLY.md](NIGHTLY.md#macos-lld-helper-and-desktop-caches).
