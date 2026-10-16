#!/usr/bin/env python3
"""Summarize retained case results locally and in GitHub Actions."""

import argparse
import json
import os
from pathlib import Path


def summarize(directory):
    results = [json.loads(path.read_text(encoding="utf-8"))
               for path in sorted(directory.glob("*/*/result.json"))]
    if not results:
        return "No compiler case results were produced; inspect the build/CTest log.\n"
    failures = [result for result in results if not result["passed"]]
    lines = [f"Compiler cases: {len(results) - len(failures)}/{len(results)} passed.", "",
             "| Group | Passed | Total |", "|---|---:|---:|"]
    for suite in ["i", "ii", "iii"]:
        subset = [result for result in results if result["case"].startswith(suite + "/")]
        lines.append(f"| {suite} | {sum(result['passed'] for result in subset)} | {len(subset)} |")
    lines += ["", f"Commit: `{results[0]['commit']}`. Seed: `{results[0]['seed']}`.",
              "Fixed expectations: pending human review."]
    if failures:
        lines += ["", "Failures:", ""]
        for result in failures:
            stages = ", ".join(failure["stage"] for failure in result["failures"])
            lines.append(f"- `{result['case']}` ({stages}); see its `result.json` and command logs.")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    summary = summarize(args.directory)
    print(summary, end="")
    args.directory.mkdir(parents=True, exist_ok=True)
    (args.directory / "summary.md").write_text(summary, encoding="utf-8")
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as output:
            output.write(summary)
