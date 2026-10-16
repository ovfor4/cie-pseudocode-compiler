"""Small contract checks for the test harness, including intentional failures."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from run import Commands, Failure, check_diagnostics, check_result, compare


class HarnessTests(unittest.TestCase):
    def test_strict_output(self):
        compare(b"a\nb\n", b"a\r\nb\r\n", "stdout")
        for actual in [b"a\nb", b"a \nb\n", b"A\nb\n", b"a\n\nb\n"]:
            with self.assertRaises(Failure):
                compare(b"a\nb\n", actual, "stdout")

    def test_exit_and_stderr(self):
        for code, stderr in [(1, b""), (-11, b""), (0, b"unexpected")]:
            with self.assertRaises(Failure):
                check_result(subprocess.CompletedProcess([], code, b"ok\n", stderr), b"ok\n")

    def test_reason_and_location_must_share_a_diagnostic(self):
        expected = [{"reason": "Expected ':'", "line": 3}]
        check_diagnostics(subprocess.CompletedProcess([], 1, b"",
            b"error: Expected ':' at line 3\ncompilation terminated.\n"), expected)
        for diagnostic in [b"error: Expected ':' at line 4", b"error: Expected ':'",
                           b"error: Expected ':' at line 30\nerror: unrelated at line 3"]:
            with self.assertRaises(Failure):
                check_diagnostics(subprocess.CompletedProcess([], 1, b"",
                    diagnostic + b"\ncompilation terminated.\n"), expected)

    def test_timeout_is_failure_and_saved(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            with self.assertRaisesRegex(Failure, "timed out"):
                Commands(directory).run([sys.executable, "-c", "import time; time.sleep(5)"],
                                        directory, timeout=0.05)
            self.assertIn('"timeout_expired": true', (directory / "01.json").read_text())


if __name__ == "__main__":
    unittest.main()
