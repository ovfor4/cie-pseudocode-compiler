# CPSC Bundle

Open a `.cps` file, enter its input in **Input**, and click **Run**. The file is saved before running. Program output and errors appear in **Output**, with compiler errors colored. Click **Stop** to end a running program.

Shortcuts use Cmd on macOS and Ctrl on Windows: **N** new, **Shift+N** new window, **O** open, **S** save, **F** find, **/** toggle line comment. The menu bar lists these commands with their shortcuts: at the top of the screen on macOS, at the top of each window on Windows. Each window has its own file, Input and Run.

Nightly provides a standalone Windows x64 `.exe` and a macOS Apple Silicon `.tar.xz` containing the `.app`. Windows extracts the included `cpsc.exe` into the current working directory on startup; macOS keeps it inside the `.app`. Windows uses the system WebView2 runtime.

For development and build instructions, see [AGENTS/DEVELOPMENT.md](AGENTS/DEVELOPMENT.md).
