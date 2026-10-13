# Installing and packaging cpsc

To build from source, follow [build.md](build.md). For command-line usage, see [cpsc.md](cpsc.md). Run build commands from the project root; package installation commands run from the extracted package directory.

## Linux installation

After configuring and building, install the compiler and manual with:

```bash
sudo make -C build install
cpsc test.cps -r
man cpsc
```

This calls `install.sh` and installs `cpsc` to `/usr/local/bin` and its manual to
`/usr/local/share/man/man1`. Installation generates the manual automatically and
requires Pandoc. Ordinary builds still do not require Pandoc.

For installation without sudo, set a writable prefix in the existing build:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
make -C build install
export PATH="$HOME/.local/bin:$PATH"
```

You can also run `./install.sh --source-dir build --prefix "$HOME/.local"`
directly after building `cpsc` and running `make -C build doc`.
`DESTDIR` is supported for staging, for example `DESTDIR=/tmp/cpsc-stage make -C build install`.

## Unix release packages

Create an installation archive with:

```bash
make -C build release
```

The output is `build/cpsc-VERSION-SYSTEM-ARCH.tar.xz`. `VERSION` uses HEAD's commit
time in UTC followed by its short ID: `yyyy-mm-dd-hh-mm-ss-commit`.
The version is read each time packaging runs. The package uses the current build's
execution mode and contains `cpsc`, `doc/cpsc.1`, `install.sh` and `LICENSE`.
Packaging builds the compiler and manual automatically; it requires Git and Pandoc.
Use `EMBEDDED` when recipients should not need LLVM or `lli` at run time.

Extract the archive with `tar -xJf`, enter the extracted directory, then run
`sudo ./install.sh`, or `./install.sh --prefix "$HOME/.local"` for a local installation.
The package installer does not require CMake, Git or Pandoc.
For a custom prefix, add its `bin` directory to PATH; `man` normally discovers the
adjacent `share/man` directory from PATH. If your man configuration overrides this,
use `man -M "$HOME/.local/share/man" cpsc` for the local prefix.

## Windows installation

Install from an existing MinGW build with:

```powershell
cmake --build build --target install
```

This is equivalent to `mingw32-make -C build install`: it generates the HTML
manual with Pandoc and calls `install.ps1`. The Windows default
`CMAKE_INSTALL_PREFIX` is `%LOCALAPPDATA%\Programs\cpsc`; an explicitly configured
prefix is preserved. To change it, run
`cmake -S . -B build -DCMAKE_INSTALL_PREFIX="D:/My Programs/cpsc"` before installing.

You can also call `install.ps1` directly. It installs for the current user without
administrator rights. Its source directory must contain `cpsc.exe` and `doc/cpsc.html`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1 -SourceDir .\build
```

The default destination is `%LOCALAPPDATA%\Programs\cpsc`: `cpsc.exe` goes in
that directory and the HTML manual goes in its `doc` subdirectory. The installer
adds the directory to the user PATH and creates Start Menu > cpsc > cpsc Help,
which opens the manual in the default browser. Open a new terminal afterward;
existing parent terminals retain their old PATH. Reinstalling does not duplicate
the PATH entry. Use `-Prefix 'D:\My Programs\cpsc'` for a custom destination,
or `-Help` for usage. Windows installation does not support `DESTDIR` staging.
The PowerShell installer targets Windows PowerShell 5.1 or later and has only
been reviewed statically; Windows installation still needs testing on Windows.

## Windows release packages

Create an installation archive from the configured MinGW build with:

```powershell
cmake --build build --target release
```

This is equivalent to `mingw32-make -C build release`. Windows produces
`build/cpsc-VERSION-Windows-ARCH.zip`, using the same UTC commit version as Unix
and retaining the configured execution mode. The ZIP contains `cpsc.exe`,
`doc/cpsc.html`, `install.ps1` and `LICENSE` under one top-level directory.
It uses CMake's default Deflate compression and needs no additional compression
tool; packaging requires Git and Pandoc, just as on Unix.

Extract the ZIP using Windows Explorer, open PowerShell in the extracted package
directory, then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1
```

Recipients need neither CMake, Git nor Pandoc to install. To check installation,
open a new terminal, run `Get-Command cpsc` and compile a source with
`cpsc test.cps`; open Start Menu > cpsc > cpsc Help to check the manual. Running
the installer again should leave only one entry for its directory in the user PATH.
