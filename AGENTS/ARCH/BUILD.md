# Build and run

The compiler emits native executables by default and textual `.ll` IR with `-S`.
Initial configuration must explicitly select `CPSC_RUN_MODE=PATH`, `EMBEDDED` or
`NONE` (no default). All modes need LLVM/Clang/LLD 23 development libraries,
Perl with core modules (including Compress::Zlib and JSON::PP) and native C/C++
development files to build; NONE supports both outputs.
The following commands use Debian/Ubuntu's versioned compiler names:

```bash
# Prepare dependencies using the recommendations in doc/build.md (see DEPENDENCIES.md).
cmake -S . -B build -DCPSC_RUN_MODE=EMBEDDED \
  -DCMAKE_C_COMPILER=clang-23 -DCMAKE_CXX_COMPILER=clang++-23
cmake --build build
build/cpsc prog.cps              # save prog.out (prog.exe on Windows) beside the source
build/cpsc prog.cps -S           # save prog.ll beside the source
build/cpsc prog.cps -r           # run without saving output
build/cpsc prog.cps -r -S -o prog.ll # save IR, then run it
build/cpsc prog.cps -o prog && ./prog
```


Generated IR uses the C++ exception ABI; native compilation links its runtime
automatically. Manually linking emitted `.ll` files still requires a C++ driver
(use LLVM 23's `clang++`, or `clang++-23` on Debian/Ubuntu).
TRY/CATCH/ENDTRY and bare THROW are compiler extensions.
Runtime checks print the source line and error reason to stderr before throwing
anonymous exceptions; the final handler in generated main
prints `Unhandled exception. Program terminated.` to stderr and returns 1
(lowering details: [RUNTIME_CHECKS_EXCEPTIONS.md](../SUBSYSTEMS/RUNTIME_CHECKS_EXCEPTIONS.md)).
[Nightly CI](NIGHTLY.md) runs compiler tests, including embedded exception execution, on
Linux, Windows and macOS with LLVM 23. Platform-specific changes require
successful checks on the native runner.
