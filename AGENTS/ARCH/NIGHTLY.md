# Nightly builds

`.github/workflows/nightly-build.yml` runs on every push to `main`, without path
filters. A shared concurrency group cancels superseded runs. A final publish job
requires all four build jobs to succeed before updating the existing `nightly-build`
release. Any build or check failure leaves all previous attachments untouched.
The publish job deletes all old uploaded assets, including paginated results,
then uploads the six packages under `latest` names, and `version.json` last. It never creates or deletes the release
and preserves its tag, title and notes. GitHub's generated
source downloads remain available.

The native compiler jobs also build CPSC Bundle, reusing their EMBEDDED executable.
Linux compiles and tests the app without publishing an installer. Windows x64
adds a standalone .exe and macOS arm64 adds a .tar.xz containing the .app,
both with per-product version filenames. Windows embeds its compiler and extracts it into
the current working directory on startup; macOS keeps the compiler inside the app.
Native runner tests check prefilled input, EOF, diagnostics, output before exit,
stopping and restart. Portable packages are checked for the desktop executable,
compiler and notices, then run with toolchain directories removed from PATH.
The publish job requires both Bundle packages alongside the four compiler and VSIX packages.
See [`bundle/AGENTS/DEVELOPMENT.md`](../../bundle/AGENTS/DEVELOPMENT.md) for the shared editor and build details.

The Linux compiler and plugin jobs use Ubuntu 24.04 x86_64; the macOS compiler
job uses a macOS 15 arm64 runner, and Windows uses Windows Server 2025 x86_64.
All four check out the push SHA with full history (per-product versions use
`git log` on each product's paths) and run independently. All three
compiler jobs use LLVM/Clang/LLD 23, Perl (build-time), Release and EMBEDDED settings, Pandoc, and the
existing `release` target. Linux obtains its toolchain from apt.llvm.org; macOS
uses Homebrew's LLVM 23 and builds matching static LLD with the helper described in
[LINKING.md](LINKING.md). Windows uses `msys2/setup-msys2@v2` with MINGW64,
MSYS2 LLVM/Clang packages, explicit native compiler and LLVM paths, and the
MinGW Makefiles generator. It downloads the official Pandoc 3.12.1 Windows
x86_64 ZIP into `build/deps/packages/` for CMake to extract. Each compiler job
extracts and checks its archive, then runs a small OUTPUT program with the
packaged binary and checks native generation plus `.ll` output without tool lookup. Windows checks its ZIP in PowerShell with PATH limited to
Windows system directories, excluding MSYS2 and LLVM. The macOS check also
verifies arm64 and rejects non-system dynamic libraries. Each job then runs its packaged installer into a temporary prefix and runs the installed compiler;
Windows also checks the Start Menu shortcut, the HKCU PATH entry (no duplicate on a second run) and a
`cmake --install` of the build tree. The plugin uses
Node.js 22, `npm ci --ignore-scripts` and
`npm run package` (which runs the plugin's syntax checks and tests before packaging),
then checks VSIX contents. Each job retains its checked package
as a separate Actions artifact for one day, failing if the package is missing.
Compiler jobs enable
BUILD_TESTING and run all CTest cases with the push SHA as seed before
their package smoke checks and uploads. Failed tests retain inputs and logs as
Actions artifacts for 14 days; test summaries appear even on failure. See [TESTING.md](../TESTING.md).
The publish job downloads only the current run's `nightly-*` artifacts and checks
that exactly one nonempty package exists for each build before any release API
calls. It then checks main's current SHA to avoid modifying nightly for a
superseded push. Release upload failures do not roll back deleted assets.
The workflow uses `GITHUB_TOKEN` with `contents: write`.
Each product (compiler, plugin, bundle) is versioned by the last commit touching its
paths, and its package names carry that commit's UTC `YYYYMMDD-HHMMSS-shorthash`.
The publish job validates the names and requires the three compiler packages to
share one version and the two bundles another, before any API call. Release assets
replace that segment with `latest` (for example `cpsc-latest-Darwin-arm64.tar.xz`,
`cpsc-plugin-latest.vsix`, `cpsc-bundle-latest-Windows-x86_64.exe`), and
`version.json` records each product's `time` and `commit`.
The VSIX internal version is `YYYYMMDD.HHMMSS.0+commit`, with leading zeros
removed from the numeric time component. Before committing workflow changes, run
actionlint, simulate release API calls, and smoke-test the packages locally;
do not contact or modify the real release for local verification.

## macOS LLD helper and desktop caches

The macOS job updates Homebrew and upgrades LLVM 23 before building LLD. It checks
that clangDriver defines `AMDGPU::setArchNameInTriple` to reject older bottles
corrupted by Homebrew's duplicate-member LTO conversion (homebrew-core #301252).
The macOS nightly job caches the helper's versioned source/build directory and still
runs the helper to configure, build incrementally and install after restoring it.
Exact cache keys include the runner image, SDK identity, LLVM package version and
compiler/library content, workspace path and helper script hash. No fallback key
restores a build from a different toolchain. Homebrew updates and the LLVM archive
symbol check still run before cache lookup. Windows and macOS desktop jobs cache
compiled Rust dependencies after selecting the toolchain, including on failure;
project crates rebuild with the current compiler payload.
