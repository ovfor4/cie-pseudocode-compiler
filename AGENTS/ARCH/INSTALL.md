# Installation, manuals and release packaging

On Linux, `make -C build install` builds the manual and calls the POSIX
`install.sh` to copy `cpsc` into `CMAKE_INSTALL_PREFIX/bin` and all manuals into
`CMAKE_INSTALL_PREFIX/share/cpsc/man/man1` and `man7` (default prefix `/usr/local`);
section 7 pages are installed together in `man7` without category subdirectories.
Linux man-db registration appends a deduplicated MANDATORY_MANPATH entry to
`/etc/manpath.config` or `/etc/man_db.conf` as root, or `~/.manpath` as a user.
macOS root registration uses `/usr/local/etc/man.d/cpsc.conf`; user installation
adds a MANPATH block to zsh/bash login configuration and prints optional sudo
commands for global registration. Existing configuration is preserved. Explicit
MANPATH settings may need updating; Linux registration rejects whitespace in the
prefix, and Unix prefixes cannot contain colons/newlines.
The script also supports `--prefix`, `--source-dir` and `DESTDIR` staging;
staging never changes host manpath or shell configuration. On macOS it removes
`com.apple.quarantine` from the installed executable if present; attribute inspection
or removal failures abort installation with a diagnostic. Ordinary builds
do not require Pandoc; `doc`, installation and release packaging do. The modular
`cmake/Manual.cmake` target defers Pandoc discovery until manual generation, prefers
the extracted executable, and sets its local runtime/data paths as needed. System
Pandoc remains the fallback.
`make -C build release` packages the executable, all manuals, installer and license
as `build/cpsc-VERSION-SYSTEM-ARCH.tar.xz`, retaining the configured execution mode.
`cmake/Release.cmake` reads, at packaging time, the last commit that touched the paths
listed in `cmake/CompilerPaths.txt`; VERSION is that commit's UTC
timestamp (`yyyymmdd-hhmmss`) followed by its short ID. Git is only required
for packaging; recipients can install the extracted archive using just the script.

The Windows `install.cmd` is a pure cmd script (no PowerShell script execution, which school
machines often block). It installs `cpsc.exe`, `doc/cpsc.html` and
`doc/language/**/*.html` for the current user under `%LOCALAPPDATA%\Programs\cpsc` (or `--prefix`);
`--source-dir` selects the artifact directory, `-h`/`--help`/`/?` print usage and unknown
arguments fail with exit 1. `DESTDIR` staging is unsupported on Windows. Prefixes must be
absolute and free of `;`, `=`, `!`, `%` and `^` (the PATH update runs with delayed expansion, which would mangle them). The script appends the directory to the user PATH
(`HKCU\Environment`, preserving REG_SZ/REG_EXPAND_SZ and never using `setx PATH`), without
copying the combined process PATH; it fails with a manual-edit message if the user PATH
contains quotation marks or would exceed about 8000 characters. A dummy `setx`/`reg delete`
pair then triggers an environment refresh. The Start Menu help shortcut
(`cpsc\cpsc Help.lnk`) is the only PowerShell use, one `powershell -Command` call; if it fails the
script warns on stderr with the `doc\cpsc.html` path and still exits 0. The script pauses only
when double-clicked (no arguments and `%cmdcmdline%` contains ` /c `).
Editing rules for `install.cmd`: ASCII only, CRLF line endings (`.gitattributes` forces
`eol=crlf`), and no `@NAME@` sequences, since `cmake/Release.cmake` copies it with
`configure_file(... @ONLY NEWLINE_STYLE CRLF)`.
The `doc` target uses CMake's `WIN32` target-platform detection: Windows generates
standalone HTML with inline styles at `build/doc/cpsc.html`; other platforms
generate `build/doc/cpsc.1`. The section 1 source is `doc/cpsc.1.md`; section 7 sources are
`doc/language/**/*.7.md`. `doc` builds every page, preserving the language
category subdirectories, as `.7` on Unix or `.html` on Windows. Sources are discovered with
CONFIGURE_DEPENDS; per-page commands regenerate only changed pages.
`cmake/ManualLinks.lua` renders cross-references as man names or local HTML links
(manual source rules: [MAN.md](../MAN.md)).
Windows `mingw32-make -C build install` builds the HTML manual and invokes
`install.cmd` through `cmd /d /c call` (the explicit `call` behaves the same on CMake 3.x and 4.x)
with the build directory and `CMAKE_INSTALL_PREFIX`. The default Windows prefix is the per-user Programs
directory above; explicit prefixes are preserved. Unix still invokes `install.sh`.
The release target explicitly passes the configured Windows flag to
`cmake/Release.cmake`, since a standalone CMake script otherwise describes the
host platform. Windows packages `cpsc.exe`, `doc/cpsc.html`, `doc/language/**/*.html`, `install.cmd` and
`LICENSE` as `build/cpsc-VERSION-Windows-ARCH.zip` using CMake's default Deflate
compression. Unix retains `.tar.xz`, the POSIX installer and the man pages.
Generated files and release packages preserve the language category layout.
Linux smoke tests can configure an isolated build with `-DWIN32=TRUE` to exercise
HTML/ZIP packaging; the resulting renamed Linux executable is only a test fixture,
not a usable Windows release.
