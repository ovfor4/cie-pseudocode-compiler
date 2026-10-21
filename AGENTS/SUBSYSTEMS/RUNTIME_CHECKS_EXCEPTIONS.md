# Runtime checks and exceptions

Part of the [subsystem notes](../SUBSYSTEMS.md). Related: [class lifetime](CLASS_LIFETIME.md).

## Runtime checks (`RuntimeCheck`)

- Array bounds, file misuse, enum ranges, null dereferences and array-parameter bounds
  contracts are checked at their emit sites.
- `ArithmeticHandler` calls `emitDivZeroCheck` before `/`, DIV and MOD; the check
  accepts integer and floating-point divisors, including both floating-point signed
  zeros.
- `emitCheck(Cond, Msg, Args)` splits the block, prints the source line and error reason
  to stderr, and calls `ExceptionHandler::emitThrow` on failure. File helper checks
  receive the source line as a runtime argument.

## Exceptions (`ExceptionHandler`)

### Handlers and calls

- Holds Context/Builder/Module refs and a map of per-function handler stacks and active
  catch depths.
- TRY pushes a catch target and an entry-block alloca for its exception pointer.
- A potentially throwing call with a handler or active catch emits an `invoke` and a
  landing pad that finishes catches being left and transfers to the nearest handler, or
  resumes unwinding when only cleanup is needed.
- CATCH uses `__cxa_begin_catch` and removes its own TRY target before emitting its
  body; normal exit and RETURN call `__cxa_end_catch`.
- Bare THROW always creates a new anonymous integer exception, using `_ZTIi` internally
  without exposing a value.
- Generated main owns the final catch target, prints the generic uncaught message, and
  returns 1.

### Cleanup and automatic destruction

- User functions enable cleanup support before their first call: loop back edges can
  make later DECLARE sites alive at earlier invokes. Without a local handler, cleanup
  landing pads end active catches and resume unwinding.
- Returns (before catch cleanup) and cleanup resumes record exit points with their
  handler/catch state; CodeGen fills reverse, Alive-guarded destruction only when the
  function is fully emitted. Normal-exit destructor calls use that saved state and may
  create further cleanup pads.
- During unwind filling, a reentrancy guard emits plain calls; a throwing destructor
  propagates and interrupts remaining cleanup instead of terminating.
- Main records only its normal return, leaving its final uncaught-exception handler free
  of automatic object destruction.
- Owned STRING values follow class cleanup. Raw array buffers are not freed and files
  are not automatically closed.

### Runtime and platforms

- Native binaries require the C++ runtime; EmbeddedRunner defines its statically linked
  ABI symbols with ORC `absoluteSymbols`, while LLJIT with JITLink manages unwind frames
  (generic EH-frame on Linux, compact-unwind or EH-frame on macOS, COFF SEH registration
  on Windows x86_64).
- Nightly CI exercises embedded exception execution on all three platforms.
