# Conventions: exceptions, compilation units and the standard library

Part of [Load-bearing conventions](../CONVENTIONS.md).

**Program exceptions use the C++ ABI; compiler diagnostics do not.**

- `ExceptionHandler` emits anonymous integer exceptions (an internal placeholder),
  `invoke`/`landingpad`/`resume`, and paired `__cxa_begin_catch`/`__cxa_end_catch`.
- Its handler stack and catch depth are keyed by LLVM function, so synthesizing file
  helpers never inherits the caller's catches.
- User calls, file runtime calls and THROW must use `ExceptionHandler::emitCall`;
  ordinary nonthrowing libc calls stay plain calls.
- RETURN must finish all active catches after evaluating its value.
- `RuntimeCheck::emitCheck` prints the source line and error reason to stderr before
  throwing. Generated main catches anything unhandled, prints the generic termination
  message to stderr, and returns 1. Explicit bare THROW prints no diagnostic itself.
- Arrays in TRY and CATCH use separate block scopes; scalars retain existing function
  scope.

**Compilation units and entry points.**

- SourceUnit borrows its AST list; it and the visibility matrix must outlive
  compile(). Select the owning unit before each pre-pass, prototype or body so
  diagnostics and name lookup use that file.
- TypeSystem's predicate filters types and their enum values; FuncSig records
  OwnerUnit.
- Rebuild Symbols from visible exports for each unit: imported constants are
  available immediately, own constants only at their declaration, and globals from
  the beginning.
- GLOBAL bodies contain only primitive scalar declarations and literal assignments;
  IsGlobal storage is an LLVM GlobalVariable and must survive FunctionGen's symbol
  reset.
- User MAIN is case insensitive, takes no parameters and returns INTEGER; its LLVM
  name is __cps_main with internal linkage. The generated i32 main wrapper calls it
  through ExceptionHandler, truncates its result and retains the final uncaught
  handler.

**Library units.** Line-leading `#LIB` marks SourceUnit::IsLibrary. RuntimeCheck uses
the externally visible i32 `__cps_user_line`; FunctionGen's shared call hook updates
it only on non-library to library calls. Runtime helper line arguments use
RuntimeCheck::lineValue too. Compiler diagnostics retain the owning file location.

**Default standard library.** The driver extracts embedded std.cps, its imported units
and matching objects to a temporary directory that survives linking/execution. User
units implicitly import std.cps; normal transitive visibility applies. -no-std
suppresses all default library inputs and objects. Explicit libraries retain the
ordinary INCLUDE/command-line rules. Source files and objects are embedded by a
build-time Perl generator. Saved -S output remains IR only.
