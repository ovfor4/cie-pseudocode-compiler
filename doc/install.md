# Installing and packaging cpsc

To build from source, follow [build.md](build.md). For command-line usage, see [cpsc.1.md](cpsc.1.md). Run build commands from the project root; package installation commands run from the extracted package directory.

The [Nightly Build](https://github.com/ovfor4/cie-pseudocode-compiler/releases/tag/nightly-build) provides EMBEDDED release archives for Linux x86_64, Windows x86_64 and macOS 15+ Apple Silicon, plus the VS Code extension. The Nightly Build assets use the stable name `latest` in place of the version (`cpsc-latest-Linux-x86_64.tar.xz`, `cpsc-latest-Darwin-arm64.tar.xz`); `version.json` lists each product's commit time and commit. The extracted folder keeps the versioned name (`cpsc-<time>-<commit>-<Platform>`). For macOS, use the Unix release package instructions below after extraction. For Windows, download `cpsc-latest-Windows-ARCH.zip` and follow the Windows release package instructions below; no LLVM or MSYS2 installation is required to run it.

## Linux installation

After configuring and building, install the compiler and manual with:

```bash
sudo make -C build install
cpsc test.cps -r
man 1 cpsc
man 7 cpsc-language-overview
man 7 cpsc-keyword-if
man 7 cpsc-function-num_to_str
```

This calls `install.sh` and installs `cpsc` to `/usr/local/bin` and its manuals to
`/usr/local/share/cpsc/man/man1` and `/usr/local/share/cpsc/man/man7`.
Installation generates all manuals automatically and requires Pandoc. Ordinary builds still do not require Pandoc.

For installation without sudo, set a writable prefix in the existing build:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
make -C build install
export PATH="$HOME/.local/bin:$PATH"
```

You can also run `./install.sh --source-dir build --prefix "$HOME/.local"`
directly after building `cpsc` and running `make -C build doc`.
`DESTDIR` is supported for staging, for example `DESTDIR=/tmp/cpsc-stage make -C build install`.

### Manual search path registration

On Linux with man-db, root installation appends a cpsc entry to
`/etc/manpath.config` (or `/etc/man_db.conf`); installation as an ordinary user
uses `~/.manpath`. Existing configuration is preserved, and repeated installation
does not add the same entry again. Linux registration requires a prefix without
whitespace. Unix prefixes cannot contain colons or newlines.

On macOS, root installation registers the directory in
`/usr/local/etc/man.d/cpsc.conf`. Without sudo, the installer appends a cpsc
MANPATH block to `~/.zprofile` for zsh or `~/.bash_profile` for bash, preserving
existing and default search paths. Open a new login terminal afterward. Other
shells receive a manual setup hint. For a user installation, global registration
is optional; the installer recommends these manual commands (adjust the path):

```bash
sudo mkdir -p /usr/local/etc/man.d
printf '%s\n' "MANPATH $HOME/.local/share/cpsc/man" | sudo tee -a /usr/local/etc/man.d/cpsc.conf
```

An explicitly set MANPATH overrides man-db configuration. If you already set it,
add the cpsc directory and retain a colon to include default system paths:

```bash
export MANPATH="$HOME/.local/share/cpsc/man:${MANPATH:-}"
```

DESTDIR staging copies files only and never changes the host's system/user
manpath or shell configuration. Register the final directory when deploying.
The installer leaves old shared-directory manuals in place; when upgrading,
remove the previous `PREFIX/share/man/man1/cpsc.1` yourself if it is obsolete.

On macOS, `install.sh` also removes `com.apple.quarantine` from the installed
executable when present. An executable without that attribute installs normally;
failure to inspect or remove the attribute is reported as an installation error.

## Unix release packages

Create an installation archive with:

```bash
make -C build release
```

The output is `build/cpsc-VERSION-SYSTEM-ARCH.tar.xz`. `VERSION` uses the UTC time
and short ID of the last commit that changed the compiler sources (the paths in
`cmake/CompilerPaths.txt`), not HEAD: `yyyymmdd-hhmmss-commit`.
The version is read each time packaging runs. The package uses the current build's
execution mode and contains `cpsc`, `doc/cpsc.1`, `doc/language/**/*.7`, `install.sh` and `LICENSE`.
Packaging builds the compiler and manual automatically; it requires Git and Pandoc.
All modes generate executables without an installed compiler, linker or development SDK. Use `EMBEDDED` when recipients should also be able to use `-r` without LLVM or `lli` installed.

Extract the archive with `tar -xJf`, enter the extracted directory, then run
`sudo ./install.sh`, or `./install.sh --prefix "$HOME/.local"` for a local installation.
The package installer does not require CMake, Git or Pandoc.
For a custom prefix, add its `bin` directory to PATH. The installer registers
`PREFIX/share/cpsc/man` in manpath configuration; it does not rely on automatic
discovery of an adjacent shared man directory. For explicit lookup, use
`man -M "$HOME/.local/share/cpsc/man" 7 cpsc-keyword-if`.

## Windows installation

Install from an existing MinGW build with:

```bat
cmake --build build --target install
```

This is equivalent to `mingw32-make -C build install`: it generates the HTML
manual with Pandoc and runs `install.cmd` through `cmd.exe`. The Windows default
`CMAKE_INSTALL_PREFIX` is `%LOCALAPPDATA%\Programs\cpsc`; an explicitly configured
prefix is preserved. To change it, run
`cmake -S . -B build -DCMAKE_INSTALL_PREFIX="D:/My Programs/cpsc"` before installing.

You can also run `install.cmd` directly. It installs for the current user without
administrator rights and needs no PowerShell script execution. Its source directory
must contain `cpsc.exe`, `doc\cpsc.html` and `doc\language\**\*.html`:

```bat
install.cmd --source-dir build
```

In PowerShell, run `.\install.cmd` instead. Quote paths that contain spaces:
`install.cmd --prefix "D:\My Programs\cpsc"`. Use `--help` for usage.

The default destination is `%LOCALAPPDATA%\Programs\cpsc`: `cpsc.exe` goes in
that directory, the command manual goes in `doc`, and syntax manuals go in
`doc\language\function` and `doc\language\keyword`; the overview stays in
`doc\language`. The command manual links to the language overview and its pages.
The installer adds the directory to the user PATH only (not the system PATH) and
creates Start Menu > cpsc > cpsc Help, which opens the manual in the default browser.
Only this shortcut uses PowerShell; if PowerShell is unavailable or blocked, the
installer prints a warning with the path to `doc\cpsc.html` and still succeeds.
Open a new terminal afterward; existing parent terminals retain their old PATH
(sign out and in if `cpsc` is still not found). Reinstalling does not duplicate
the PATH entry.

Limits: `DESTDIR` staging is unsupported, the prefix must be an absolute path
without `;`, `=`, `!`, `%` or `^`, and a user PATH that contains quotation marks or is longer than
about 8000 characters cannot be updated by the installer; add the directory by hand
in that case. CI runs the installer on Windows; double-click behaviour and the
Explorer PATH refresh still need a manual desktop check.

## Windows release packages

Create an installation archive from the configured MinGW build with:

```bat
cmake --build build --target release
```

This is equivalent to `mingw32-make -C build release`. Windows produces
`build/cpsc-VERSION-Windows-ARCH.zip`, using the same UTC commit version as Unix
and retaining the configured execution mode. The ZIP contains `cpsc.exe`,
`doc/cpsc.html`, `doc/language/**/*.html`, `install.cmd` and `LICENSE` under one top-level directory.
It uses CMake's default Deflate compression and needs no additional compression
tool; packaging requires Git and Pandoc, just as on Unix.

Extract the whole ZIP using Windows Explorer, then double-click `install.cmd` in the
extracted package directory, or run it from Command Prompt (`install.cmd`) or
PowerShell (`.\install.cmd`). Double-clicking keeps the window open until a key is pressed.

Recipients need neither CMake, Git nor Pandoc to install. To check installation,
open a new terminal, run `where cpsc` and compile a source with
`cpsc test.cps`; open Start Menu > cpsc > cpsc Help to check the manual. Running
the installer again should leave only one entry for its directory in the user PATH.
