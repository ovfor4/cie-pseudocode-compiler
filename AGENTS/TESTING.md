# Compiler testing

Compiler tests live in `tests/`; run instructions are in its README. `BUILD_TESTING` defaults to OFF and needs only Perl (core modules) when enabled. Python remains a compiler build dependency. CMake builds independent C++ reference executables with the same host ABI and registers one CTest entry per `case.json`, plus harness and generator checks.

Each case directory has `program.cps` and `case.json`. Suite i adds `expected.txt`, optional `input.txt`, input files under `files/`, expected text-file contents under `expected-files/`, and independently written trace notes with human-review status. Suite ii adds `reference.cc` and `generate.pl`; the generator accepts seed and random count, emits the total record count followed by one ASCII record per line, and each program emits one result line per record. Suite iii stores an `errors` list of `{reason: regex, line: integer}` in its manifest. A cause and its line must appear on the same error diagnostic. EOF after a final newline is the following source line.

Multi-file cases specify `sources` as an ordered array of paths relative to the
case directory, defaulting to `["program.cps"]`. Library sources can be in nested
subdirectories. Both compilation and runner commands receive the whole list.
Suite iii error expectations may also specify `file` relative to the case directory;
the reason, line and exact absolute input path must match the same diagnostic.
The harness contract tests reject wrong filenames, filename prefixes and split
reason/location messages.

Suite i and ii cases may list `objects` as `{source, output}` pairs, for example `lib.c` to `lib.o`. The runner copies the case directory beside the results, builds each object there with the host C (`.c`) or C++ (`.cc`) compiler passed by `--cc` and `--cxx`, and compiles `sources` from the copy so `INCLUDE_OBJ "lib.o"` resolves. C++ objects may use the embedded C++ runtime in EMBEDDED mode; the JIT lazily loads the captured standard-library and compiler-support archives while retaining the host exception ABI. PATH mode depends on the external lli runtime configuration.

Optional manifest fields are `exit_code` for expected program status (default 0) and `timeout` for execution/reference seconds (default 30; file-intensive cases use 180). Compilation has a 120-second timeout. Do not make a known compiler defect pass by changing the expectation, adding an exemption or comparing only a permissive substring of program output.

The Perl runner (`tests/run.pl`) accepts `--case`, `--cpsc`, `--run-mode`, `--perl`, `--reference`, `--seed`, `--count` and `--results`. CMake provides paths, including executable suffixes, and caches `CPSC_TEST_SEED` and `CPSC_TEST_COUNT`. The default seed is HEAD read at execution, not configure time. CI overrides it with the checked-out push SHA. Both native and enabled runner paths are tested independently; NONE skips only runner execution. Windows CI must configure a native `PERL_EXECUTABLE` (Strawberry Perl) because MSYS perl cannot read the native `D:/` drive paths CMake passes.

All commands run without a shell and retain their exact argv, working directory, input, status and output under the case result directory. Each execution path has its own files. Existing results for that case are replaced on re-run. Avoid concurrent runs of the same case into the same results directory. Tests compare CRLF-normalized bytes without stripping whitespace. A case may add `expected-stderr.txt` for runtime diagnostics, compared exactly; without it, successful compilation and ordinary programs require empty stderr.

Perl generators use Random.pm's SHA-256 sequence with explicit big-endian extraction, fixed group identity and decimal counter. `generator-vectors.json` pins 100 records plus boundaries using the all-zero 40-character seed; update a vector only when deliberately changing a generator, and verify repeatability and a different seed. C++ references must avoid undefined behavior and implement the specification independently. Use arithmetic closed forms or standard containers where they provide an independent oracle. Record platform-dependent C behavior as such; never call it CIE behavior.

Before committing a test change, build the affected references and run the affected CTest cases. Compiler defects may fail: inspect retained diagnostics and document the failure accurately. Before committing workflow changes, run actionlint and retain the existing release API simulation/package smoke checks without contacting the real release. No compiler implementation changes belong in a tests-only task.
