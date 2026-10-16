# Compiler tests

Enable tests in an existing configured build (Perl with core Digest::SHA is additionally required):

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --parallel 2
python3 tests/summarize.py build/test-results
```

Use `ctest --test-dir build -L '^ii$' --output-on-failure` for one suite, or `-R '^i/arrays$'` for one case. CTest returns failure if any case fails; it continues through the other cases. [Historical failures and coverage gaps](FAILURES.md) are intentionally visible, with no expected-failure exemptions.

The suites are **i**: independently traced fixed outputs, **ii**: C++ differential tests with Perl inputs, and **iii**: rejected programs with required causes and source lines. Fixed expectations are **pending human review**; each case's notes explain the trace and distinguish CIE rules from project extensions. REAL calculations are observed through BOOLEAN/INTEGER output; REAL text and REAL-to-string formatting remain outside this suite.

The default is 10,000 random records per differential group, plus fixed boundary records. Both generated native programs and the configured `-r` path are checked; NONE builds only run native programs. Each source is compiled once per execution path and processes its records in one batch. Tests normalize only CRLF to LF; spaces, blank lines, case and final newlines remain significant. Generated text files use the same comparison rule.

Replay an exact seed, or reduce the random count for a quick local check:

```sh
cmake -S . -B build -DCPSC_TEST_SEED=FULL_COMMIT_SHA -DCPSC_TEST_COUNT=100
ctest --test-dir build -L '^ii$' --output-on-failure
```

Use the actual SHA from a failed report in place of `FULL_COMMIT_SHA`. Clear `CPSC_TEST_SEED` to return to HEAD-at-execution defaults. CI explicitly sets it to the push SHA; generators never fetch main or use Perl rand. Reproduction requires the same source revision, seed and count. Native C type results may legitimately differ between ABIs; they are compared to the C++ executable built on that platform.

`build/test-results/<suite>/<case>/` contains `result.json`, numbered command metadata, stdin/stdout/stderr, expected output and isolated work directories. Random mismatches also save the first differing record. Re-running a case replaces only that case's directory; summaries include any other retained results, so remove `build/test-results` before collecting a fresh full-suite report. Commands have finite timeouts (30 seconds normally, 120 for compilation, 180 for file-intensive executions); timeout and launch/crash failures fail the case.

In Actions, each compiler platform runs all three suites and summarizes results before its existing package smoke checks and upload. Failures retain inputs and logs as an Actions artifact for 14 days. Each platform publishes only after its own tests pass. The initial Linux failures have been fixed; any failing compiler test still blocks publication for that platform. The independent plugin job can publish separately.

For adding cases and runner interfaces, see [AGENTS/TESTING.md](../AGENTS/TESTING.md).
