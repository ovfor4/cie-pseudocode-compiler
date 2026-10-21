# Embedded execution (EMBEDDED mode)

- In EMBEDDED mode, `-r` uses LLJIT with JITLink in-process
  (`tools/driver/EmbeddedRunner.cc`). It clones CodeGen's module into an independent
  `ThreadSafeContext`, sets the JIT target triple/DataLayout, and uses the Large
  code model with PIC. CodeGen retains its original module and context.
  Static exception ABI symbols resolve to the host runtime through ORC
  `absoluteSymbols`; ordinary C symbols use process lookup. Linux uses generic
  EH-frame registration; macOS uses LLVM's compact-unwind or EH-frame support.
  Windows x86_64 compatibility lives in `tools/driver/MinGWJIT.cc`, exposed through
  two internal entry points for object-layer creation and archive symbol interfaces.
  Symbol-less MinGW `.pdata$NAME`/`.xdata$NAME` COMDATs become associative COMDATs
  of `.text$NAME` in private object copies. Weak absolute-zero `__cxa_pure_virtual`
  references resolve to the host ABI, with their fallback kept local. Archive
  interfaces and object emission use the same normalization.
  LLVM's `.pdata` liveness and associative unwind sections are retained. A graph-local
  `__ImageBase` is shared by relocations and SEH registration; nearby pointer-jump
  stubs handle external direct CALL/JMP and unwind personality references, preserving
  data references. Each graph combines `.pdata` and `.pdata$NAME` into one sorted,
  allocation-owned table, validated before `RtlAddFunctionTable`. Allocation actions
  call `RtlDeleteFunctionTable` on cleanup before memory release.
  MinGW runtime bindings remain in EmbeddedRunner: formatting aliases, no-op `__main`,
  host stack guard/failure handler, `_write`, `__acrt_iob_func`, stdio locks and the
  x86_64 `___chkstk_ms` probe. Prefer `msvcrt.dll` before process-wide C lookup so
  JIT code uses compatible `FILE` objects. MinGW supplies `_lock_file`/`_unlock_file`
  because Microsoft's msvcrt.dll does not export them; Wine can mask this difference.
  See [Microsoft's x64 unwind requirements](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64),
  [LLVM 23's COFF graph builder](https://github.com/llvm/llvm-project/blob/release/23.x/llvm/lib/ExecutionEngine/JITLink/COFFLinkGraphBuilder.cpp)
  and [MinGW's stdio lock implementation](https://github.com/mingw-w64/mingw-w64/blob/master/mingw-w64-crt/stdio/mingw_lock.c).
  Initialize the JIT before looking up `main` and deinitialize while its code is
  still mapped. Execution failures return 1 without `compilation terminated.`.
  No external runner or PATH lookup is used; `-r` alone runs in memory.
  Platform-specific changes require native CI verification ([BUILD.md](BUILD.md)).

The embedded JIT lazily loads the captured C++ standard-library and compiler-support archives using StaticLibraryDefinitionGenerator. Members defining exception ABI or unwind symbols are filtered so all exception frames use the host runtime; additional host ABI functions are exposed explicitly. Archive buffers own their extracted bytes for the JIT lifetime.
