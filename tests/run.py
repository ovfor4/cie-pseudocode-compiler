#!/usr/bin/env python3
"""Run one compiler fixture. Only CRLF is normalized; failures are never waived."""

import argparse
import difflib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent


class Failure(Exception):
    pass


def normalized(data):
    return data.replace(b"\r\n", b"\n")


def compare(expected, actual, label):
    expected, actual = normalized(expected), normalized(actual)
    if expected == actual:
        return
    # repr keeps trailing spaces, missing newlines and non-UTF-8 bytes visible.
    left = [repr(line) + "\n" for line in expected.splitlines(keepends=True)]
    right = [repr(line) + "\n" for line in actual.splitlines(keepends=True)]
    diff = "".join(difflib.unified_diff(left, right, "expected", "actual", n=2))
    raise Failure(f"{label} differs:\n{diff[:5000]}")


def check_result(result, expected, exit_code=0):
    errors = []
    if result.returncode != exit_code:
        errors.append(f"exit code: expected {exit_code}, got {result.returncode}")
    if result.stderr:
        errors.append(f"unexpected stderr: {result.stderr[:2000]!r}")
    try:
        compare(expected, result.stdout, "stdout")
    except Failure as error:
        errors.append(str(error))
    if errors:
        raise Failure("\n".join(errors))


def check_diagnostics(result, expectations):
    errors = []
    if result.returncode != 1:
        errors.append(f"compiler exit code: expected 1, got {result.returncode}")
    if result.stdout:
        errors.append(f"unexpected stdout: {result.stdout[:1000]!r}")
    lines = normalized(result.stderr).decode("utf-8", errors="replace").splitlines()
    for expectation in expectations:
        matching = [line for line in lines if "error:" in line
                    and re.search(expectation["reason"], line)]
        location = rf"\bline {expectation['line']}\b"
        if not any(re.search(location, line) for line in matching):
            errors.append(f"missing error /{expectation['reason']}/ at line "
                          f"{expectation['line']}; matching diagnostics: {matching}")
    if lines.count("compilation terminated.") != 1:
        errors.append("expected exactly one 'compilation terminated.' line")
    if errors:
        raise Failure("\n".join(errors))


class Commands:
    def __init__(self, directory):
        self.directory = directory
        self.index = 0

    def run(self, command, cwd, data=b"", timeout=30):
        self.index += 1
        stem = self.directory / f"{self.index:02d}"
        metadata = {"command": [str(arg) for arg in command], "cwd": str(cwd),
                    "timeout": timeout}
        stem.with_suffix(".stdin").write_bytes(data)
        try:
            result = subprocess.run(metadata["command"], cwd=cwd, input=data,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    timeout=timeout)
            metadata["exit_code"] = result.returncode
        except subprocess.TimeoutExpired as error:
            metadata["timeout_expired"] = True
            stem.with_suffix(".stdout").write_bytes(error.stdout or b"")
            stem.with_suffix(".stderr").write_bytes(error.stderr or b"")
            raise Failure(f"command timed out after {timeout}s: {command}") from error
        except OSError as error:
            metadata["launch_error"] = str(error)
            raise Failure(f"cannot launch {command}: {error}") from error
        finally:
            stem.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n",
                                                 encoding="utf-8")
        stem.with_suffix(".stdout").write_bytes(result.stdout)
        stem.with_suffix(".stderr").write_bytes(result.stderr)
        return result


def workspace(case_dir, directory, name):
    work = directory / name
    work.mkdir()
    if (case_dir / "files").is_dir():
        shutil.copytree(case_dir / "files", work, dirs_exist_ok=True)
    return work


def check_files(case_dir, work):
    for expected in sorted((case_dir / "expected-files").glob("**/*")):
        if expected.is_file():
            relative = expected.relative_to(case_dir / "expected-files")
            actual = work / relative
            if not actual.is_file():
                raise Failure(f"missing output file: {relative}")
            compare(expected.read_bytes(), actual.read_bytes(), str(relative))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", required=True, type=Path)
    parser.add_argument("--cpsc", required=True, type=Path)
    parser.add_argument("--run-mode", choices=["EMBEDDED", "PATH", "NONE"], required=True)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--perl", default="perl")
    parser.add_argument("--seed", default="")
    parser.add_argument("--count", type=int, default=10000)
    parser.add_argument("--results", type=Path, default=ROOT / "build/test-results")
    args = parser.parse_args(argv)
    if args.count < 1:
        parser.error("--count must be positive")
    args.case = args.case.resolve()
    args.cpsc = args.cpsc.resolve()
    if args.reference:
        args.reference = args.reference.resolve()
    case_dir = args.case.parent
    suite, name = case_dir.parent.name, case_dir.name
    config = json.loads(args.case.read_text(encoding="utf-8"))
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                     text=True, timeout=10).strip()
    seed = args.seed or commit
    directory = args.results.resolve() / suite / name
    if directory.exists():
        shutil.rmtree(directory)
    directory.mkdir(parents=True)
    commands = Commands(directory)
    report = {"case": f"{suite}/{name}", "commit": commit, "seed": seed,
              "random_count": args.count, "platform": platform.platform(),
              "run_mode": args.run_mode, "failures": []}
    source = case_dir / "program.cps"

    def attempt(label, action):
        try:
            action()
            print(f"PASS {suite}/{name} [{label}]", flush=True)
        except (Failure, OSError, ValueError) as error:
            report["failures"].append({"stage": label, "message": str(error)})
            print(f"FAIL {suite}/{name} [{label}]: {error}", flush=True)

    def diagnostics():
        for output_format in ["native", "ir"]:
            work = workspace(case_dir, directory, output_format)
            target = work / ("program.exe" if os.name == "nt" else "program.bin")
            flags = ["-S"] if output_format == "ir" else []
            for existing in [False, True]:
                sentinel = b"existing output must survive\n"
                if existing:
                    target.write_bytes(sentinel)
                result = commands.run([args.cpsc, source, *flags, "-o", target], work,
                                      timeout=120)
                # Check output preservation even when the diagnostics are wrong.
                preservation_error = None
                if existing and (not target.exists() or target.read_bytes() != sentinel):
                    preservation_error = "compilation failure overwrote existing output"
                if not existing and target.exists():
                    preservation_error = "compilation failure created output"
                    target.unlink()
                def validate():
                    if preservation_error:
                        raise Failure(preservation_error)
                    check_diagnostics(result, config["errors"])
                attempt(f"{output_format}/{'existing' if existing else 'absent'}", validate)

    try:
        if suite == "iii":
            diagnostics()
        else:
            data = (case_dir / "input.txt").read_bytes() if (case_dir / "input.txt").exists() else b""
            if suite == "ii":
                if not args.reference:
                    raise Failure("differential tests require --reference")
                generated = commands.run([args.perl, case_dir / "generate.pl", seed,
                                          str(args.count)], directory)
                if generated.returncode != 0 or generated.stderr:
                    raise Failure(f"generator failed: {generated.stderr!r}")
                data = generated.stdout
                records = normalized(data).splitlines()
                if not records or int(records[0]) != len(records) - 1:
                    raise Failure("generator must emit record count followed by one record per line")
                work = workspace(case_dir, directory, "reference")
                reference = commands.run([args.reference], work, data,
                                         timeout=config.get("timeout", 30))
                if reference.returncode != 0 or reference.stderr:
                    raise Failure(f"reference failed ({reference.returncode}): {reference.stderr!r}")
                expected = reference.stdout
                if len(normalized(expected).splitlines()) != len(records) - 1:
                    raise Failure("reference must emit exactly one output line per input record")
            else:
                expected = (case_dir / "expected.txt").read_bytes()
            (directory / "input.txt").write_bytes(data)
            (directory / "expected.txt").write_bytes(expected)

            def execute(mode):
                work = workspace(case_dir, directory, mode)
                if mode == "native":
                    target = work / ("program.exe" if os.name == "nt" else "program.bin")
                    compiled = commands.run([args.cpsc, source, "-o", target], work, timeout=120)
                    check_result(compiled, b"")
                    command = [target]
                else:
                    command = [args.cpsc, source, "-r"]
                result = commands.run(command, work, data, timeout=config.get("timeout", 30))
                if suite == "ii" and normalized(result.stdout) != normalized(expected):
                    wanted, got = normalized(expected).splitlines(), normalized(result.stdout).splitlines()
                    index = next((i for i in range(min(len(wanted), len(got)))
                                  if wanted[i] != got[i]), min(len(wanted), len(got)))
                    record = records[index + 1] if index + 1 < len(records) else b"<extra output>"
                    print(f"First differing record {index + 1}: {record!r}", flush=True)
                    (directory / f"{mode}-first-difference.json").write_text(json.dumps({
                        "record": index + 1, "input": record.decode("ascii"),
                        "expected": repr(wanted[index]) if index < len(wanted) else "<missing>",
                        "actual": repr(got[index]) if index < len(got) else "<missing>"}, indent=2) + "\n",
                        encoding="utf-8")
                check_result(result, expected, config.get("exit_code", 0))
                check_files(case_dir, work)
            for mode in ["native"] + (["runner"] if args.run_mode != "NONE" else []):
                attempt(mode, lambda mode=mode: execute(mode))
    except (Failure, OSError, ValueError) as error:
        report["failures"].append({"stage": "setup", "message": str(error)})
        print(f"FAIL {suite}/{name} [setup]: {error}", flush=True)
    report["passed"] = not report["failures"]
    (directory / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Seed: {seed}; results: {directory}", flush=True)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
