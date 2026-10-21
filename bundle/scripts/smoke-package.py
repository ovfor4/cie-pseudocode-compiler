"""Check a portable desktop package without relying on an installed toolchain."""

from pathlib import Path
import os
import subprocess
import sys
import tempfile
import time

root = Path(sys.argv[1]).resolve()
environment = os.environ.copy()
environment["PATH"] = os.pathsep.join((os.path.join(environment["SystemRoot"], "System32"), environment["SystemRoot"])) if sys.platform == "win32" else "/nonexistent"
with tempfile.TemporaryDirectory(prefix="cpsc bundle smoke ") as directory:
    work = Path(directory)
    if sys.platform == "win32":
        assert root.is_file() and root.suffix == ".exe", "Expected a single portable .exe"
        # Extraction happens before WebView startup and publishes a complete file by rename.
        desktop = subprocess.Popen([str(root)], cwd=work, env=environment)
        compiler = work / "cpsc.exe"
        try:
            deadline = time.monotonic() + 30
            while not compiler.is_file():
                assert desktop.poll() is None, "Desktop exited before extracting its compiler"
                assert time.monotonic() < deadline, "Compiler extraction timed out"
                time.sleep(0.1)
        finally:
            if desktop.poll() is None:
                desktop.terminate()
            desktop.wait(timeout=10)
    else:
        compilers = list(root.rglob("cpsc"))
        assert len(compilers) == 1, f"Expected one packaged compiler, found {compilers}"
        compiler = compilers[0]
        assert compiler.with_name("cpsc-bundle").is_file(), "Missing desktop executable"
        resources = compiler.parent.parent / "Resources" if sys.platform == "darwin" else compiler.parent
        for filename in ("LICENSE", "ThirdPartyNotices.md", "licenses/vscode.txt", "licenses/plugin.md"):
            assert (resources / filename).is_file(), f"Missing {filename}"
    (work / "data.txt").write_text("relative file\n", encoding="utf-8")
    source = work / "source file.cps"
    source.write_text('DECLARE N : INTEGER\nINPUT N\nOUTPUT N\nDECLARE S : STRING\nOPENFILE "data.txt" FOR READ\nREADFILE "data.txt", S\nCLOSEFILE "data.txt"\nOUTPUT S\n', encoding="utf-8")
    result = subprocess.run([str(compiler), str(source), "-r"], input=b"17\n", capture_output=True, cwd=work, env=environment, timeout=30)
    assert result.returncode == 0, result.stderr
    assert result.stdout.replace(b"\r\n", b"\n") == b"17\nrelative file\n", result.stdout
    assert result.stderr == b"", result.stderr
print("Desktop package smoke passed: portable application, compiler, stdin and relative files.")
